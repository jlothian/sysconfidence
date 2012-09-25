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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "config.h"
#include "orbtimer.h"
#include "comm.h"

#ifdef SHMEM
	#include <mpp/shmem.h>
	# ifndef _SHMEM_COLLECT_SYNC_SIZE
	//    FIXME:  hacks to cope with Cray....
	#     define _SHMEM_COLLECT_SYNC_SIZE _SHMEM_REDUCE_SYNC_SIZE
	#     define _my_pe my_pe
	#     define _num_pes num_pes
	# endif
	static long cSync[_SHMEM_COLLECT_SYNC_SIZE];
	static long rSync[_SHMEM_REDUCE_SYNC_SIZE];
#else
	#include <mpi.h>
#endif

/**
 * \brief routines in this file abstract the communication layer.
 *
 * so far, only MPI is implemented
 *
 */

/** 
 * \brief Creates a buffer struct to hold the communication info
 * \param nbytes Tells the size of the data array
 * \return p Buffer variable
 */
buffer_p comm_newbuffer(size_t nbytes) {
	buffer_p p;
	p = (buffer_p) malloc(sizeof(buffer_t));
	assert(p != NULL);
	p->len = nbytes;
#ifdef SHMEM
	p->data = (void *)shmalloc(nbytes);
#else	/* MPI */
	p->data = (void *)malloc(nbytes);
#endif
	assert(p->data != NULL);
	return p;
}

/**
 * \brief Frees the buffer struct
 * \param buf Buffer to be free'd
 * \sa comm_newbuffer
 */
void comm_freebuffer(buffer_p buf) {
#ifdef SHMEM
	shfree(buf->data);
#else	/* MPI */
	free(buf->data);
#endif
	free(buf);
	return;
}

/**
 * \brief Allocates space for the dist array  
 * \param num_bins The size of the dist array
 */
uint64_t *comm_alloc_dist(size_t num_bins) {
	uint64_t *dist_array;
#ifdef SHMEM
	int i;
	dist_array = (uint64_t *) shmalloc(num_bins * sizeof(uint64_t));
	assert(dist_array != NULL);
	for (i = 0; i < num_bins; i++) {
		dist_array[i] = 0x0;
	}
#else	/* MPI */
	dist_array = (uint64_t *) calloc(num_bins, sizeof(uint64_t));
#endif
	return dist_array;
}

/**
 * \brief Frees the dist array
 * \param dist_array Memory location to be free'd
 * \sa comm_alloc_dist
 */
void comm_free_dist(uint64_t *dist_array) {
#ifdef SHMEM
	shfree(dist_array);
#else	/* MPI */
	free(dist_array);
#endif
}

/**
 * \brief Collects the measurements into a global location
 * \param g The global array of measurements collected over the course of the run
 * \param l The local array of measurements
 */
void comm_aggregate(measurement_p g, measurement_p l) {
	/* collects local measurments into a global measurement */
	int i, ierr;
	assert(l->nbins == g->nbins);
	assert(l->num_histograms == g->num_histograms);
	ierr = 0;
#ifdef SHMEM
	int max = (l->nbins/2 + 1) > _SHMEM_REDUCE_MIN_WRKDATA_SIZE ? (l->nbins/2 + 1) : _SHMEM_REDUCE_MIN_WRKDATA_SIZE;
	uint64_t *pWrk = (uint64_t *)shmalloc(max * sizeof(uint64_t));
	assert(pWrk != NULL);
	
	for (i = 0; i < l->num_histograms; i++) {
		shmem_barrier_all();
		shmem_longlong_sum_to_all((long long *)g->hist[i].dist, (long long *)l->hist[i].dist,
					  l->nbins, 0, 0, num_ranks, (long long *)pWrk, rSync);
	}
	
	shmem_barrier_all();
	shfree(pWrk);
#else				/* MPI case */
	for (i = 0; i < l->num_histograms; i++) {
		ierr += MPI_Allreduce(l->hist[i].dist, g->hist[i].dist, l->nbins,
				MPI_INTEGER8, MPI_SUM, MPI_COMM_WORLD);
	}

	assert(ierr == 0);
#endif
	return;
}

/**
 * \brief Prints out the mapping of the test results
 * \param tst Holds the information used for printing
 */
void comm_showmapping(test_p tst) {
	int i;
	FILE *Fmapping;
	char fname[512];
	ROOTONLY {
		if (tst->rank_mapping == 1) {
			snprintf(fname, 512, "%s/%s.RankToNodeMapping.%dof%d",
					tst->case_name, "global", my_rank, num_ranks);
			Fmapping = fopen(fname, "w");
			fprintf(Fmapping, "#Task Rank, node_ids, (from RootTask)\n");
			for (i = 0; i < num_ranks; i++)
				fprintf(Fmapping,"%d, %ld\n", i, node_id[i]);
			fclose(Fmapping);
		}
	}
	return;
}

/**
 * \brief Gives the value of the calling node
 * \return id Is the ID of the node
 */
uint64_t comm_getnodeid() {
	/* 
	 * Assuming the system's nodes are "numbered" after some fashion, this
	 * function returns a unique integer for each hardware 'node'.  The
	 * nodeid's of two MPI ranks are compared and the communication is
	 * assumed to be local (ie. at a latency potentially less than that of
	 * the network) if the nodeid's are the same.  If something other than
	 * digits are used to differentiate the nodes this function should be
	 * replaced with a function which generates a useful nodeid for the
	 * target machine. If none of the NODEID_<TYPE> macros are set, we
	 * punt, and return nodeid's suggesting every rank on a separate node.
	 */
	uint64_t id;
#if defined(NODEID_GETHOSTNAME) || defined(NODEID_MPI) || defined(NODEID_SLURM)
	char *pn, *pp, *limit;
	int  ierr = 0;
#    if defined(SHMEM)
	gethostname(namebuff, NAMEBUFFSIZE);
	nodename = namebuff;
#    elif defined(NODEID_MPI)
	int len = NAMEBUFFSIZE;
	ierr = MPI_Get_processor_name(namebuff, &len);
	nodename = namebuff;
#    elif defined(NODEID_GETHOSTNAME)
	gethostname(namebuff, NAMEBUFFSIZE);
	nodename = namebuff;
#    elif defined(NODEID_SLURM)
	nodename = getenv("SLURM_NODEID");
#    endif
	limit = nodename + strlen(nodename);
	pp = nodename - 1;
	pn = nid;
	/* copy all (and only) the digits to a new buffer */
	while (++pp < nodename + strlen(nodename))
		if (isdigit(*pp))
			*pn++ = *pp;
	*pn = (char)0;
	errno = 0;
	id = strtoull(nid, NULL, 0);
	ierr = errno;
	if (ierr == ERANGE || ierr == EINVAL) {
		perror("strtol");
		exit(ierr);
	}
#else			/* NODEID_<TYPE> not defined. Punt. One MPI rank per node. */
	int tmp;
#    ifdef SHMEM
	tmp = _my_pe();
#    else
	int ierr = 0;
	ierr = MPI_Comm_rank(MPI_COMM_WORLD, &tmp);
	assert(ierr == 0);
#    endif
	id = tmp;
#endif			/* NODEID_<TYPE> */
	return id;
}

/**
 * \brief Returns the next power of 2 greater than n 
 * \param n 2^c > n, 2^(c-1) <= n
 * \return c  2^c > n, 2^(c-1) <= n
 */
int comm_ceil2(int n) {
	int c = 1;
	while (c < n)
		c *= 2;
	return c;
}

/** 
 * \brief Initializes the communication arrays - SHMEM
 * \param tst Will tell the test how many stages it should run
 * \param argc Typical C variable: tells the number of arguments in the command line
 * \param argv Typical C variable: holds the command line arguments
 */
void comm_SHMEM_initialize(test_p tst, int *argc, char **argv[]) {
#ifdef SHMEM
	uint64_t mynodeid;
	int i;
	
	start_pes(0);
	my_rank = _my_pe();
	num_ranks = _num_pes();
	
	root_rank = 0;
	tst->num_stages = comm_ceil2(num_ranks);
	node_id = (uint64_t *)shmalloc(num_ranks * sizeof(uint64_t));
	assert(node_id != NULL);
	for ( i=0; i<num_ranks; i++) node_id[i] = 0;
	
	/* sync arrays for collectives */
	for (i = 0; i < _SHMEM_COLLECT_SYNC_SIZE; i++) cSync[i] = _SHMEM_SYNC_VALUE;
	for (i = 0; i < _SHMEM_REDUCE_SYNC_SIZE; i++)  rSync[i] = _SHMEM_SYNC_VALUE;

	shmem_barrier_all();
	
	node_id[my_rank] = mynodeid = comm_getnodeid();
	// FIXME:
	// would prefer collect64, but it doesn't exist on some platforms. is shmem_longlong_sum_to_all() better?
	// shmem_collect64((void *)node_id, (void *)&mynodeid, 1, 0, 0, num_ranks, cSync);
	int max = (1+num_ranks/2)>_SHMEM_REDUCE_MIN_WRKDATA_SIZE ? (1+num_ranks/2) : _SHMEM_REDUCE_MIN_WRKDATA_SIZE;
	uint64_t *pWrk = (uint64_t *)shmalloc(max * sizeof(uint64_t));
	assert(pWrk != NULL);
	shmem_longlong_sum_to_all((long long *)node_id, (long long *)node_id, num_ranks, 0, 0, num_ranks, pWrk, rSync);
	
	shmem_barrier_all();
#endif	/* SHMEM */
	return;
}

/**
 * \brief Frees up the space used by comm - SHMEM
 * \sa comm_SHMEM_initialize
 */
void comm_SHMEM_finalize() {
#ifdef SHMEM
	shfree(node_id);
#endif
	/* SHMEM requires no finalize function */
	return;
}

/**
 * \brief Initializes the communication arrays for comm - MPI
 * \param tst Will tell the test how many stages it should run
 * \param argc Typical C variable: tells the number of arguments in the command line
 * \param argv Typical C variable: holds the command line arguments
 */
void comm_MPI_initialize(test_p tst, int *argc, char **argv[]) {
#ifndef SHMEM
	uint64_t mynodeid;
	int ierr = 0;

	ierr += MPI_Init(argc, argv);
	ierr += MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	ierr += MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);

	root_rank = 0;
	tst->num_stages = comm_ceil2(num_ranks);
	node_id = (uint64_t *)malloc(num_ranks * sizeof(uint64_t));
	assert(node_id != NULL);

	node_id[my_rank] = mynodeid = comm_getnodeid();
	ierr += MPI_Allgather(&mynodeid, sizeof(uint64_t), MPI_BYTE,
			      node_id, sizeof(uint64_t), MPI_BYTE, MPI_COMM_WORLD);
	assert(ierr == 0);
#endif
	return;
}

/**
 * \brief Frees up the spave used by comm - MPI
 * \sa comm_MPI_initialize
 */
void comm_MPI_finalize() {
#ifndef SHMEM
	free(node_id);
	MPI_Finalize();
#endif
	return;
}


