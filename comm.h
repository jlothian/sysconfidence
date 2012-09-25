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


#include "types.h"

#ifndef HAVE_COMM_H
#define HAVE_COMM_H


/**************************************************************
 * FUNCTION PROTOTYPES
 **************************************************************/
buffer_p comm_newbuffer(size_t nbytes);
void comm_freebuffer(buffer_p buf);
uint64_t *comm_alloc_dist(size_t num_bins);
void comm_free_dist(uint64_t *dist_array);
void comm_aggregate(measurement_p g, measurement_p l);
void comm_showmapping(test_p tst);
uint64_t comm_getnodeid();
int comm_ceil2(int n);

/* generic interface to MPI/SHMEM initialize and finalize */
#ifdef SHMEM
	#define comm_initialize	comm_SHMEM_initialize
	#define comm_finalize	comm_SHMEM_finalize
#else
	#define comm_initialize	comm_MPI_initialize
	#define comm_finalize	comm_MPI_finalize
#endif


/* MPI internal */
void comm_MPI_initialize(test_p tst, int *argc, char **argv[]);
void comm_MPI_finalize();

/* SHMEM internal */
void comm_SHMEM_initialize(test_p tst, int *argc, char **argv[]);
void comm_SHMEM_finalize();


/* communication variables */
int my_rank;
int root_rank;
int num_ranks;
uint64_t *node_id;
char *nodename;
char namebuff[NAMEBUFFSIZE];
char nid[NAMEBUFFSIZE];

#define ROOTONLY   if(my_rank == root_rank)


#endif				/* HAVE_COMM_H */

