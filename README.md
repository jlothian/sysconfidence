Usage: systemconfidence [TEST] [COMMON OPTIONS] [[NET OPTIONS]|[IO OPTIONS]]

TEST:
	 -t net        	 run the network latency test (confidence)
	 -t bit        	 run the network bit test to check for network bit errors
			 (errors will be printed to stdout as they are detected)
	 -t io         	 run the I/O test (XDD)

COMMON OPTIONS:
	 -N <casename> 	 name directory for output (default: OUTPUT_DIRECTORY)
	 -r            	 save the rank-to-node mapping in a file for later use
	 -l            	 switch from (default) linear binning to logarithmic binning (recommended)
	 -w <binwidth> 	 width of FIRST histogram bin in seconds
	 -m <time>     	 reset maximum message time to bin (log binning only)
	 -n <bins>     	 number of bins in histograms

NET/BIT OPTIONS:
	 -B <buflen>   	 buffer length for message tests in bytes
	 -C <cycles>   	 number of cycles of all-pairs collections
	 -G <messages> 	 total number of global messages to be exchanged (net only)
	 -M <messages> 	 number of messages to exchange per pair (net only)
	 -W <warmup>   	 number of warm-up messages before timing (net only)

IO OPTIONS:
	 -X <xdd_args> 	 pass arguments to XDD for the IO test (eg. -X '-target /dev/null')\n");
			 NOTE: If 'RANK' is included as part of a target name, it will be\n");
			 replaced with the rank of the process that reads/writes the target.\n");
			 Run `systemconfidences -t io` to see a list of XDD options.

For tips on building see the build.sh script.

For tips on running see the regression.bash script.

Latency Test FAQs:

Q: How do I run a simple small message latency test?

A: To test the network latency for small (8byte) payloads on the
   network, have systemconfidence walk through the schedule of each
   possible pair of processors ten times (cycles), having each pair
   exchange 10000 messages with a 1000 message warmup:

	mpirun -n $NUMPROCS     ./sysconfidence -t net -l -B 8 -C 10 -M 10000 -W 1000


Q: Should SystemConfidence be run with one task per node or several tasks per node?

A: You should test each of possibilities you might expect to run
   in a production job, since one would expect the latency to vary
   depending on how many tasks are contending for access to the
   network.  Thus:

	aprun  -n $NUMPROCS     ./sysconfidence -t net -l -B 8 -C 10 -M 10000 -W 1000
	aprun  -n $NUMPROCS -N1 ./sysconfidence -t net -l -B 8 -C 10 -M 10000 -W 1000
	
   Here, we ask for $NUMPROCS processors in both cases, but in the
   second case, we ask that these processes be placed one per node.
   It should be expected that on-node latency will be better than
   off-node latency in the first test, and the off-node latency in
   the second test is expected to be better than the off-node latency
   in the first test due to lower contention for the NIC(s).

Q: Does the latency test need to run across the whole system?  

A: No, in fact, it is desirable to run the benchmark on a representative
   subset of the system, since for the network latency test, the
   runtime is proportional to the number of communicating tasks,
   the number of cycles, and the number of messages.  When we say
   representative subset, we mean that the subset should include
   the basic feature of the system's network topology, ie. it should
   not be run inside a single node or within a single rack, but
   rather across several racks.  For a Cray XT system, we've generally
   found that running across 2-16 racks is sufficient.

Q: How can I control the execution time?

A: The execution time is proprotional to #tasks * #cycles * #messages
   (including the warmup messages).  So, you can keep the execution
   time roughly constant by keeping the product of these three
   numbers roughly constant, ie, if you double the number of tasks,
   halve the number of cycles. However, be aware that the warmup
   exists to help "mask off" de-synchronization between tasks.
   Reducing or eliminating the warmup, or reducing the number of
   test messages, may expose these synchronization issues between
   tasks in your measurements.  (you may see some extra spikes in
   the output histograms)

Q: What do the output files represent?

A: The output files contain three different representations of
   likelihood as a function of latency, ie. how likely am I to
   observe a particular latency value. The representations are
   (1) The histogram of collected data, which shows how many
       times a latency was observed within a particular bin.
       These values will vary with the number of samples.
   (2) The empirical probability density function (PDF), which is a
       non-dimensionalized version of the histogram. If you 
       integrate the area under this curve between two latencies,
       the result will tell you the probability of a measured
       latency falling in that range. Integrating from 0 to 
       infinity gives 1.0 or 100%.
   (3) The cummulative distribution function (CDF), which is the
       integral of the PDF from 0 to some latency 't'.  This tells
       us what percentage of the communications complete with a
       latency less than or equal to 't'.
   (4) A set of statistics for each histogram of data. These include:
   	minimum: the latency typically claimed by a marketing department
	mode: the latency that you'll see most often
	median: the latency which half of the samples fall above/below
	mean: the average latency
	maximum: the latency that can have the worst performance impact
	The remain values represent moments about the ordinate for the
	PDF curves:
	R1: mean again
	R2: 2nd root of variance: standard deviation  (ie. R1 +/- R2)
	R3: 3rd root of skewness **
	R4: 4th root of kurtosis **
	Mean: mean again
	Variance/skewness/kurtosis: additional statistical metrics 
	These can be used to compare networks, with those networks
	exhibiting lower R1/R2/R3/R4 scaling better than those with
	higher values for the corresponding metrics. 

Q: Within an output file, what are the columns?

A: 
   - #bin --    The bin number in the histogram.
   (us) to (us) -- The min and max latency representing the edges
                   of this bin.
   - timer --   the number of occurances or CDF/PDF value for the timer.
              The benchmark measures and bins back-to-back timer calls
	      so that the user may observe the contribution of the 
	      timing calls to the measurements.
   - onNd-OS -- on-node-one-sided measurements. this is the number of
              occurences or CDF/PDF value for the time taken by the
	      communication function on one side of the communication
	      for an on-node communication partner.
   - onNd-PW -- on-node-pairwise measurements. this is the number of
              occurences or CDF/PDF value for the time take by the
	      communication functions on both sides of the communication
	      for an on-node communication, halved for comparison to
	      the one-sided time.
   - onNd-OS-min -- the histogram/CDF/PDF representing the best one-
  	      sided latency for each block of on-node messages.
   - onNd-PW-min -- the histogram/CDF/PDF representing the best pairwise
  	      latency for each block of messages. This is the best
	      estimate for the one-way latency, and graphing this value
	      may show the "steps" present in the memory topology if
	      they are significant.
   - offNd-OS -- on-node-one-sided measurements. this is the number of
              occurences or CDF/PDF value for the time taken by the
	      communication function on one side of the communication
	      for an off-node communication partner.
   - offNd-PW -- on-node-pairwise measurements. this is the number of
              occurences or CDF/PDF value for the time take by the
	      communication functions on both sides of the communication
	      for an off-node communication, halved for comparison to
	      the one-sided time.
   - offNd-OS-min -- the histogram/CDF/PDF representing the best one-
  	      sided latency for each block of off-node messages.
   - offNd-PW-min -- the histogram/CDF/PDF representing the best pairwise
  	      latency for each block of messages. This is the best
	      estimate for the one-way latency, and graphing this value
	      may show the "steps" present in the network topology if
	      they are significant.

Q: What do I do with the CDF/PDF files?

A: These files (and the histograms) are designed as input to GNUplot.
   Because of the statistical nature of the data and the large
   variations in scale for the target systems, log-log charts should
   be used (otherwise you just see a mode-spike). Some useful charts:

   1. The off node pairwise CDFs of machines 'A' and 'B' on one
       chart to compare latencies. 
   2. The off node pairwise and pairwise minimum CDFs for machine 'A'
       on one chart to compare actual behavior to best case.
   3. The on/off node PDFs (OS or PW) to identify interruptions or
       patterns in latency delays, and to visualise the 'tails' on the
       probability curves. (esp note this should be viewed log-log)

Bit Error Test FAQs:

Q: How do I test that the network is delivering bits without errors?

A: The bit error test runs very similarly to the network latency
   test, walking through each possible pairing of communicating
   tasks several times (cycles). For each pair of tasks, the buffer
   (size specified by user) will be filled with an 8bit pattern,
   exchanged between the tasks and checked for bit errors, then the
   next pattern will be tested. All 256 8bit patterns are checked
   for each processor pair. Longer more exhaustive runs can be
   configured by increasing the number of cycles. Neither warmup,
   nor binning is performed.  Thus for a 256 task test:

	mpirun -n 256 ./sysconfidence -t bit -r -B 64 -C 1000 


