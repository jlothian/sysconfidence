/*
  This file is part of SystemConfidence.

  Copyright (C) 2012, UT-Battelle, LLC.

  This product includes software produced by UT-Battelle, LLC under Contract No. 
  DE-AC05-00OR22725 with the Department of Energy. 

  This program is free software; you can redistribute it and/or modify
  it under the terms of the New BSD 3-clause software license (LICENSE). 
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
  LICENSE for more details.

  For more information please contact the SystemConfidence developers at: 
  systemconfidence-info@googlegroups.com

*/


#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>

#include "tests.h"
#include "types.h"
#include "options.h"
#include "comm.h"
#ifdef USE_XDD
#include "xdd_main.h"
#endif

/**
 * defaults for command line options
 */
void setdefaults(test_p tst) {
	tst->num_messages = 100000;	/* messages per cycle */
	tst->num_cycles = 10;		/* cycles -- future: time limit the data collection */
	tst->num_warmup = 100;	/* keep this < 1% of tst->num_messages */
	/* tst->total_messages = (uint64_t)(tst->num_cycles) *
		(uint64_t)(tst->num_messages) * (uint64_t)(num_ranks-1); */
	tst->buf_len = 1;		/* small message */
	tst->num_bins = 1000;		/* with log binning, don't need much more */
	tst->bin_size = 50.0e-9;	/* 50ns works well with x86_64 assm timers */
	tst->log_binning = 0;		/* linear binning */
	tst->rank_mapping = 0;
	tst->test_type = 0;		/* no test defined */
	strcpy(tst->case_name, "OUTPUT_DIRECTORY");	/* user should replace */
	tst->argc = 0;
	tst->argv = NULL;
	/* file for this rank's timestamp dump (only for io_test) */
	tst->tsdump = NULL;
}

/**
 * \brief parse command line options without seatbelts/sanity/consistency checks.
 * \return number of arguments found
 */
void general_options(test_p tst, int argc, char *argv[]) {
	int ierr, opt, printhelp;
	extern char *optarg;
	extern int optind, opterr, optopt;
	setdefaults(tst);
	printhelp = 0;
	ierr = 0;

	/* parse test names and common options */
	while ((opt = getopt(argc, argv, "t:m:n:w:N:lrhB:C:G:M:W:X:")) != -1) {
		switch (opt) {
			case 't':
				if (strcmp(optarg,"net")==0) {
					tst->test_type = NET_TEST;
				} else if (strcmp(optarg,"bit")==0) {
					tst->test_type = BIT_TEST;
#ifdef USE_XDD
				} else if (strcmp(optarg,"io")==0) {
					tst->test_type = IO_TEST;
#endif
				} else {
					fprintf(stderr,"Test %s unrecognized!\n",optarg);
					ierr++;
				}
				break;
			case 'l':
				tst->log_binning = 1;
				break;
			case 'r':
				tst->rank_mapping = 1;
				break;
			case 'n':
				tst->num_bins = strtol(optarg, NULL, 0);
				if (tst->num_bins == 0)
					ierr++;
				break;
			case 'w':
				tst->bin_size = strtod(optarg, NULL);
				if (tst->bin_size <= 0.0)
					ierr++;
				break;
			case 'm':
				tst->max_hist_time = strtod(optarg, NULL);
				if (tst->max_hist_time <= 0.0)
					ierr++;
				break;
			case 'N':
				strncpy(tst->case_name, optarg, NAMEBUFFSIZE);
				if (strlen(tst->case_name) == 0)
					ierr++;
				break;
			case 'h':
				printhelp = 1;
				break;
			case 'B':
				tst->buf_len = strtol(optarg, NULL, 0);
				if (tst->buf_len == 0)
					ierr++;
				break;
			case 'C':
				tst->num_cycles = strtol(optarg, NULL, 0);
				if (tst->num_cycles == 0)
					ierr++;
				break;
			case 'M':
				tst->num_messages = strtol(optarg, NULL, 0);
				if (tst->num_messages == 0)
					ierr++;
				break;
			case 'W':
				tst->num_warmup = strtol(optarg, NULL, 0);
				if (tst->num_warmup == 0)
					ierr++;
				break;
			case 'X':
				parse_xdd_args(tst, optarg, argv[0]);
				break;
			default: /* ? */
				ierr++;
				break;
		}
	}

	/* if there was a parsing error, or if user asked for help */
	if (ierr != 0 || printhelp) {
		ROOTONLY print_help(tst, argv[0]);
		exit(1);
	}

	/* no test was specified */
	if (tst->test_type == 0) {
		ROOTONLY {
			fprintf(stderr,"You must specify a test to run!\n\n");
			print_help(tst, argv[0]);
		}
		exit(1);
	}

	/* if no options were passed for the IO test, just pass program name */
	if ((tst->test_type==IO_TEST) && (tst->argc==0)) {
		parse_xdd_args(tst, "", argv[0]);
	}

	if (tst->log_binning == 1) {
		tst->max_hist_time = 1.0;
		tst->hist_scale = ((double)tst->num_bins) / log(tst->max_hist_time / tst->bin_size);
	} else {		/* LINEAR binning */
		tst->hist_scale = tst->num_bins * tst->bin_size;
		tst->max_hist_time = tst->num_bins * tst->bin_size;
	}
}


/** \brief tokenizes xdd argument list */
void parse_xdd_args(test_p tst, char *optarg, char *progname) {
	char delim = ' ';
	char *ptr;
	int i,count;

	/* one for program name */
	count=1;
	/* one if there is at least one argument*/
	if (strlen(optarg) > 0) count++;
	/* get number of occurences of delim */
	for (i=0; i<strlen(optarg); i++)
		if (optarg[i] == delim) count++;
	/* allocate count + 3 for '-ts dump prefix' + 4 for output and errout + 1 for NULL */
	tst->argv = malloc((count+3+4+1)*sizeof(char *));
	assert(tst->argv);
	/* copy program name */
	tst->argv[0] = progname;

	/* tokenize string and store elements in argv */
	char delims[2] = { delim,'\0'};
	ptr = strtok(optarg, delims);
	for (i=1; (i < count) && (ptr != NULL); i++) {
		tst->argv[i] = ptr;
		ptr = strtok(NULL, delims);
	}
	tst->argc = i;

	/* make sure user didn't add -ts dump themselves */
	for (i=1; i<tst->argc; i++) {
		/* make sure user didn't add -ts dump themselves */
		if ( (strcmp(tst->argv[i],"-ts")==0||strcmp(tst->argv[i],"-timestamp")==0)
				&& strcmp(tst->argv[i+1],"dump")==0) {
			ROOTONLY {
				fprintf(stderr,"You can not specify '-ts dump' in the XDD arguments\n");
				fprintf(stderr,"This option is used by SystemConfidence to gather timestamps.\n\n");
			}
			exit(1);
		}
		/* make sure user didn't add -output or -errout */
		if (strcmp(tst->argv[i],"-output")==0||strcmp(tst->argv[i],"-errout")==0) {
			ROOTONLY {
				fprintf(stderr,"You can not specify '-output' or '-errout' in the XDD arguments\n");
				fprintf(stderr,"This option is used by SystemConfidence to redirect output.\n\n");
			}
			exit(1);
		}
		/* no need to move forward if they asked for help */
		if (strcmp(tst->argv[i],"-fullhelp")==0 ||
				strcmp(tst->argv[i],"-h")==0 ||
				strcmp(tst->argv[i],"-help")==0 ) {
			tst->argv[tst->argc]=NULL;
			return;
		}
		/* replace "RANK" with my_rank in target name */
		if (strcmp(tst->argv[i],"-target")==0) {
			char *ptr = strstr(tst->argv[i+1],"RANK");
			if (ptr != NULL)
				snprintf(ptr,5,"%04d",my_rank);
		}
	}

	tst->tsdump = malloc(FNAMESIZE);
	snprintf(tst->tsdump, FNAMESIZE, "%s/tsdump.rank.%04d", tst->case_name, my_rank);
	/* add -output option */
	tst->argv[tst->argc] = "-output";
	tst->argc++;
	tst->argv[tst->argc] = malloc(FNAMESIZE);
	snprintf(tst->argv[tst->argc], FNAMESIZE, "%s.stdout", tst->tsdump);
	tst->argc++;
	/* add -errout option */
	tst->argv[tst->argc] = "-errout";
	tst->argc++;
	tst->argv[tst->argc] = malloc(FNAMESIZE);
	snprintf(tst->argv[tst->argc], FNAMESIZE, "%s.stderr", tst->tsdump);
	tst->argc++;
	/* add -ts dump option */
	tst->argv[tst->argc] = "-ts";
	tst->argc++;
	tst->argv[tst->argc] = "dump";
	tst->argc++;
	tst->argv[tst->argc] = tst->tsdump;
	tst->argc++;
	/* list must end with null */
	tst->argv[tst->argc]=NULL;
}


void print_help(test_p tst, char *progname) {
	setdefaults(tst);
	fprintf(stderr,	"Usage: %s [TEST] [COMMON OPTIONS] [[NET OPTIONS]|[IO OPTIONS]]\n\n", progname);
	fprintf(stderr, "TEST:\n");
	fprintf(stderr, "\t -t net        \t run the network latency test (confidence)\n");
	fprintf(stderr, "\t -t bit        \t run the network bit test\n");
#ifdef USE_XDD
	fprintf(stderr, "\t -t io         \t run the I/O test (XDD)\n");
#endif
	fprintf(stderr, "COMMON OPTIONS:\n");
	fprintf(stderr, "\t -N <casename> \t name directory for output (default: %s)\n", tst->case_name);
	fprintf(stderr, "\t -r            \t save the rank-to-node mapping\n");
	fprintf(stderr, "\t -l            \t switch from (default) linear binning to logarithmic binning\n");
	fprintf(stderr, "\t -w <binwidth> \t width of FIRST histogram bin in seconds (default: %g)\n", tst->bin_size);
	fprintf(stderr, "\t -m <time>     \t reset maximum message time to bin (log binning only)\n");
	fprintf(stderr, "\t -n <bins>     \t number of bins in histograms (default: %d)\n", tst->num_bins);
	fprintf(stderr, "NET/BIT OPTIONS:\n");
	fprintf(stderr, "\t -B <buflen>   \t buffer length for message tests in bytes (default: %d)\n", tst->buf_len);
	fprintf(stderr, "\t -C <cycles>   \t number of cycles of all-pairs collections (default: %d)\n", tst->num_cycles);
        /* Don't list this option for now, may add back later */
	/* fprintf(stderr, "\t -G <messages> \t total number of global messages to be exchanged\n"); */
	fprintf(stderr, "\t -M <messages> \t number of messages to exchange per pair (default: %d)\n", tst->num_messages);
	fprintf(stderr, "\t -W <warmup>   \t number of warm-up messages before timing (default: %d)\n", tst->num_warmup);
#ifdef USE_XDD
	fprintf(stderr, "IO OPTIONS:\n");
	fprintf(stderr, "\t -X <xdd_args> \t pass arguments to XDD for the IO test (eg. -X '-target /dev/null')\n");
	fprintf(stderr, "\t\t\t NOTE: If 'RANK' is included as part of a target name, it will be\n");
	fprintf(stderr, "\t\t\t replaced with the rank of the process that reads/writes the target.\n");
	fprintf(stderr, "\t Run `%s -t io` to see a list of XDD options.\n",progname);
#endif
	fprintf(stderr, "\n");
}


