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

/**
 * \brief Bit test for Confidence
 *
 * Checks bit-wise integrity of network exchanges
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

#include "config.h"
#include "orbtimer.h"
#include "comm.h"
#include "tests.h"
#include "measurement.h"

#ifdef SHMEM
	#include <mpp/shmem.h>
#else
	#include <mpi.h>
#endif

/* number of histograms */
#define BIT_LEN 0

/* histograms for measurements */
enum bit_vars {
	null
};

char *bit_labels[] = {
	""
};


/** 
 * \brief Creates the struct for the measurement data
 * \param tst Struct that tells the number of cycles and stages to run the test.
 * \param label Label for the measurement struct
 * \return m The measurement struct
 */
measurement_p bit_measurement_create(test_p tst, char *label) {
	int i;
	measurement_p m = measurement_real_create(tst, label, BIT_LEN);
	for (i = 0; i < BIT_LEN; i++)
		strncpy(m->hist[i].label,bit_labels[i],LABEL_LEN);
	return m;
}


/*************************************************************************************************
 * for each cycle through the possible communication partners, we will exchange the test patterns.
 *************************************************************************************************/

/**
 * \brief Check to make sure the test is correct, SHMEM
 * \param tst Struct that tells the number of cycles and stages to run the test.
 * \param m Struct that holds the results of the test.
 */
void bit_SHMEM_test(test_p tst, measurement_p m) {
#ifdef SHMEM
	buffer_t *abuf, *bbuf, *cbuf;
	int i, j, k, icycle, istage, partner_rank;
	unsigned char pattern;
	abuf = comm_newbuffer(m->buflen);							/* set up exchange buffers */
	bbuf = comm_newbuffer(m->buflen);
	cbuf = comm_newbuffer(m->buflen);
	for (icycle = 0; icycle < tst->num_cycles; icycle++) {					/* multiple cycles repeat the test */
		for (istage = 0; istage < tst->num_stages; istage++) {				/* step through the stage schedule */
			partner_rank = my_rank ^ istage;					/* who's my partner for this stage? */
			shmem_barrier_all();
			if ((partner_rank < num_ranks) && (partner_rank != my_rank)) {		/* valid pair? proceed with test */
				for (k=0x00; k< 0x100; k++) {		/* try each byte patter */
					pattern=k;
					for (i=0; i<m->buflen; i++) ((unsigned char *)(abuf->data))[i]=pattern;
					shmem_putmem(bbuf->data, abuf->data, m->buflen, partner_rank);
					shmem_fence();
					shmem_getmem(cbuf->data, bbuf->data, m->buflen, partner_rank);
					for (i=0; i<m->buflen; i++) {
						if (((unsigned char *)(cbuf->data))[i] != pattern) {
							printf("DATA ERROR DETECTED:   node:%20s   rank:%10d"
								"   pattern:0x%2x   buflen:%10d   position:%10d\n",
								nodename, my_rank, (int)pattern, m->buflen, i);
						} /* if mismatch */
					} /* for buflen */
				} /* for pattern */
			} /* if valid pairing */
		} /* for istage */
	} /* for icycle */
	shmem_barrier_all();
	comm_freebuffer(cbuf);
	comm_freebuffer(bbuf);
	comm_freebuffer(abuf);
#endif
	return;
}


/*************************************************************************************************
 * for each cycle through the possible communication partners, we will exchange the test patterns.
 *************************************************************************************************/

/** 
 * \brief Check to make sure the test is correct, MPI
 * \param tst Struct that tells the number of cycles and stages to run the test.
 * \param m Struct that holds the results of the test.
 */
void bit_MPI_test(test_p tst, measurement_p m) {
#ifndef SHMEM
	MPI_Status mpistatus;
	buffer_t *abuf, *bbuf, *cbuf;
	int i, j, k, icycle, istage, ierr, partner_rank;
	unsigned char pattern;
	abuf = comm_newbuffer(m->buflen);							/* set up exchange buffers */
	bbuf = comm_newbuffer(m->buflen);
	cbuf = comm_newbuffer(m->buflen);
	for (icycle = 0; icycle < tst->num_cycles; icycle++) {					/* multiple cycles repeat the test */
		for (istage = 0; istage < tst->num_stages; istage++) {				/* step through the stage schedule */
			partner_rank = my_rank ^ istage;					/* who's my partner for this stage? */
			ierr = MPI_Barrier(MPI_COMM_WORLD);
			if ((partner_rank < num_ranks) && (partner_rank != my_rank) && (partner_rank >= 0)) {		/* valid pair? proceed with test */
				for (k=0x00; k<0x100; k++) {		/* try each byte pattern */
					pattern=k;
					for (i=0; i<m->buflen; i++) ((unsigned char*)(abuf->data))[i]=pattern;
					ierr  = MPI_Sendrecv(abuf->data, m->buflen, MPI_BYTE, partner_rank, 0,
							     bbuf->data, m->buflen, MPI_BYTE, partner_rank, 0,
							     MPI_COMM_WORLD, &mpistatus);
					ierr += MPI_Sendrecv(bbuf->data, m->buflen, MPI_BYTE, partner_rank, 0,
							     cbuf->data, m->buflen, MPI_BYTE, partner_rank, 0,
							     MPI_COMM_WORLD, &mpistatus);
					for (i=0; i<m->buflen; i++) {
						if (((unsigned char *)(cbuf->data))[i] != pattern) {
							printf("DATA ERROR DETECTED:   node:%20s   rank:%10d   pattern:0x%2x   buflen:%10d   position:%10d\n",
										nodename, my_rank, (int)pattern, m->buflen, i);
						} /* if mismatch */
					} /* for buflen */
				} /* for pattern */
			}/* if valid pairing */
		} /* for istage */
	} /* for icycle */
	ierr = MPI_Barrier(MPI_COMM_WORLD);
	comm_freebuffer(cbuf);
	comm_freebuffer(bbuf);
	comm_freebuffer(abuf);
#endif
	return;
}
