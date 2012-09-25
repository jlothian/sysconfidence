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


#ifndef _TESTS_H
#define _TESTS_H

#include "types.h"

enum {UNDEF=0, NET_TEST=1, BIT_TEST=2, IO_TEST=3};

/**************************************************************
 * FUNCTIONS
 **************************************************************/
#ifdef SHMEM
	#define bit_test bit_SHMEM_test
	#define net_test net_SHMEM_test
	#define io_test io_SHMEM_test
#else
	#define bit_test bit_MPI_test
	#define net_test net_MPI_test
	#define io_test io_MPI_test
#endif

/* network latency test */
void 		net_SHMEM_test(test_p tst, measurement_p m);
void 		net_MPI_test(test_p tst, measurement_p m);
void 		net_measurement_bin(test_p tst, measurement_p m, double *t, double *cos, double *cpw, int LOCAL);
measurement_p 	net_measurement_create(test_p tst, char *label);

/* network bit exchange test */
void 		bit_SHMEM_test(test_p tst, measurement_p m);
void 		bit_MPI_test(test_p tst, measurement_p m);
measurement_p 	bit_measurement_create(test_p tst, char *label);

/* ifdef XDD because the API isn't stable */
#ifdef USE_XDD
/* io test */
void 		io_SHMEM_test(test_p tst, measurement_p m);
void		io_MPI_test(test_p tst, measurement_p m);
void		io_measurement_bin(test_p tst, measurement_p m, double *dtimes);
measurement_p	io_measurement_create(test_p tst, char *label);
#endif

#endif /* _TESTS_H */

