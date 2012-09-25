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

#include "config.h"
#include "orbtimer.h"
#include "comm.h"
#include "tests.h"
#include "measurement.h"
#include "xdd_main.h"

#ifdef SHMEM
	#include <mpp/shmem.h>
#else
	#include <mpi.h>
#endif

/* number of io histograms */
#define IO_LEN 2

/* histograms for io measurements */
enum io_vars {
	diskOp, diskOpMinimum
};

/* histogram labels */
char *io_labels[] = {
	"diskOp", "diskOpMinimum"
};


/* file ending added to timestamp dumps by XDD */
#define TSDUMP_POSTFIX "target.0000.bin"

/* converts xdd pclk time to seconds, given:
 * p (pclk value) and r (timer resolution) */
#define pclk2sec(p,r) ((double)p/(double)r/1.0e9)
/* Bytes Per Unit... don't set to 0 */
#define BPU 2^20

/* returns 1 if x is expected operation... 0 if not */
#define READ_OP(x)  (x==OP_TYPE_READ||x==SO_OP_READ)
#define WRITE_OP(x) (x==OP_TYPE_WRITE||x==SO_OP_WRITE||x==SO_OP_WRITE_VERIFY)
#define NO_OP(x)    (x==OP_TYPE_NOOP||x==SO_OP_NOOP)
#define EOF_OP(x)   (x==OP_TYPE_EOF||x==SO_OP_EOF)

/* magic number to make sure we can read the file */
#define BIN_MAGIC_NUMBER 0xDEADBEEF

/* Read an XDD binary timestamp dump into a structure */
int io_xdd_readfile(char *filename, tthdr_t **tsdata, size_t *tsdata_size);

/**
 \brief Creates the measurement struct used to run the test
 \param tst Will tell the test how many times to run
 \param label The label for the measurement struct
 \return m The measurement struct
 */
measurement_p io_measurement_create(test_p tst, char *label) {
	int i;
	measurement_p m = measurement_real_create(tst, label, IO_LEN);
	for (i = 0; i < IO_LEN; i++)
		strncpy(m->hist[i].label,io_labels[i],LABEL_LEN);
	return m;
}

/**
 \brief Runs the test - SHMEM
 \param tst Tells how many times to run
 \param m Holds the information collected over the course of the run
 \sa io_MPI_test
*/
void io_SHMEM_test(test_p tst, measurement_p m) {
	/* SHMEM and MPI are the same for the io test */
	io_MPI_test(tst, m);
	return;
}

/**
 \brief Runs the IO test - MPI
 \param tst Tells the test how many times to run
 \param m Holds the measurement data collected over the course of the run
*/
void io_MPI_test(test_p tst, measurement_p m) {
	int i,retval;
	/* filename to open after xdd_main */
	char fname[FNAMESIZE];
	/* timestamp data structure and vars */
	tthdr_t *tsdata = NULL;
	size_t tsdata_size = 0;
	int64_t res,numents;
	/* length of ops in seconds */
	double *disk_times;

	/* run xdd with provided arguments */
	xdd_main(tst->argc, tst->argv);
	/* if user passed a help option, no need for analysis */
	for (i=0; i<tst->argc; i++) {
		if (strcmp(tst->argv[i],"-h")==0)
			return;
		if (strcmp(tst->argv[i],"-help")==0)
			return;
		if (strcmp(tst->argv[i],"-fullhelp")==0)
			return;
	}

	/* get timestamp data structure */
	snprintf(fname, FNAMESIZE, "%s.%s", tst->tsdump, TSDUMP_POSTFIX);
	retval = io_xdd_readfile(fname, &tsdata, &tsdata_size);
	/* did the file read successfully? */
	if (retval == 0) {
		fprintf(stderr,"Could not read XDD timestamp dump: %s\n",fname);
		return;
	}

	/* xdd timer resolution and distribution size */
	res = tsdata->res;
	numents = tsdata->tt_size;

	/* skip empty sets */
	if (numents <= 0) {
		fprintf(stderr,"XDD timestamp dump is empty: %s\n",fname);
		return;
	}

	/* alloc time array */
	disk_times = malloc(sizeof(double)*numents);
	assert(disk_times);

	/* convert xdd time values to seconds */
	int64_t j;
	for (j = 0; j < numents; j++) {
		disk_times[j] = pclk2sec((tsdata->tte[j].disk_end - tsdata->tte[j].disk_start),res);
	}

	/* init/fix variables in test struct */
	tst->num_stages=1;
	tst->num_warmup=0;
	tst->num_cycles=1;
	tst->num_messages=numents;
	tst->buf_len=tsdata->blocksize;

	/* bin the times */
	io_measurement_bin(tst, m, disk_times);

	free(disk_times);
}

/**
 \brief Converts the time measurements to bin
 \param tst Used by the time2bin function
 \param m List of measurements
 \param dtimes List of times the tests ran for 
*/
void io_measurement_bin(test_p tst, measurement_p m, double *dtimes) {
	double dtmin = dtimes[0];
	uint64_t *pop, *pmin;
	int i;

	/* dist arrays */
	pop = m->hist[diskOp].dist;
	pmin = m->hist[diskOpMinimum].dist;

	for (i = 0; i < tst->num_messages; i++) {
		if (dtimes[i] >= 0.0) {
			/* bin the disk op */
			pop[time2bin(tst,dtimes[i])]++;
			/* save the minimum */
			if (dtimes[i] < dtmin)
				dtmin = dtimes[i];
		}
	}

	/* bin the min */
	if (dtmin > 0.0)
		pmin[time2bin(tst,dtmin)]++;
}

/*********************************************************
 * \brief Read an XDD binary timestamp dump into a structure
 *
 * IN:
 *  \param  filename    - name of the .bin file to read
 * OUT:
 *  \param  tsdata      - pointer to the timestamp structure
 *  \param  tsdata_size - size malloc'd for the structure
 * RETURN:
 *  \return  1 if succeeded, 0 if failed
 *********************************************************/
int io_xdd_readfile(char *filename, tthdr_t **tsdata, size_t *tsdata_size) {

	FILE *tsfd;
	size_t tsize = 0;
	tthdr_t *tdata = NULL;
	size_t result = 0;
	uint32_t magic = 0;

	/* open file */
	tsfd = fopen(filename, "rb");

	if (tsfd == NULL) {
		fprintf(stderr,"Can not open file: %s\n",filename);
		return 0;
	}

	/* get file length */
	fseek(tsfd,0,SEEK_END);
	tsize = ftell(tsfd);
	fseek(tsfd,0,SEEK_SET);

	if (tsize == 0) {
		fprintf(stderr,"File is empty: %s\n",filename);
		return 0;
	}

	/* allocate memory */
	tdata = malloc(tsize);
	assert(tdata);

	if (tdata == NULL) {
		fprintf(stderr,"Could not allocate memory for file: %s\n",filename);
		return 0;
	}

	/* check magic number in tthdr */
	result = fread(&magic,sizeof(uint32_t),1,tsfd);
	fseek(tsfd,0,SEEK_SET);

	if (result == 0) {
		fprintf(stderr,"Error reading file: %s\n",filename);
		return 0;
	}
	if (magic != BIN_MAGIC_NUMBER) {
		fprintf(stderr,"File is not in a readable format: %s\n",filename);
		return 0;
	}

	/* read rest of structure */
	result = fread(tdata,tsize,1,tsfd);

	if (result == 0) {
		fprintf(stderr,"Error reading file: %s\n",filename);
		return 0;
	}

	*tsdata = tdata;
	*tsdata_size = tsize;

	/* close file */
	fclose(tsfd);

	return 1;
}
