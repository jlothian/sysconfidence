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


#define LINUX 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "xdd/src/base/xdd.h"

/* make sure MAX is defined */
#ifndef MAX
#define MAX(a,b) (a > b ? a : b)
#endif
/* converts xdd pclk time to seconds, given:
 * p (pclk value) and r (timer resolution) */
#define pclk2sec(p,r) ((float)p/(float)r/1.0e9)
/* Bytes Per Unit... don't set to 0 */
#define BPU 2^20

/* returns 1 if x is expected operation... 0 if not */
#define READ_OP(x)  (x==OP_TYPE_READ||x==SO_OP_READ)
#define WRITE_OP(x) (x==OP_TYPE_WRITE||x==SO_OP_WRITE||x==SO_OP_WRITE_VERIFY)
#define NO_OP(x)    (x==OP_TYPE_NOOP||x==SO_OP_NOOP)
#define EOF_OP(x)   (x==OP_TYPE_EOF||x==SO_OP_EOF)

/* magic number to make sure we can read the file */
#define BIN_MAGIC_NUMBER 0xDEADBEEF

/* output file, if specified */
#define OUTFILENAME_LEN 512
char outfile_prefix[OUTFILENAME_LEN];


/* write all the outfiles */
void write_outfiles(tthdr_t *src, tthdr_t *dst, tte_t ***src_thread,
		tte_t ***dst_thread, int32_t nthreads, int64_t *num_ops);
/* read, check, and store the src and dst file data */
int xdd_getdata(char *file1, char *file2, tthdr_t **src, tthdr_t **dst);
/* Read an XDD binary timestamp dump into a structure */
int xdd_readfile(char *filename, tthdr_t **tsdata, size_t *tsdata_size);
/* parse command line options */
int getoptions(int argc, char **argv);
/* print command line usage */
void printusage(char *progname);


int main(int argc, char **argv) {

	int fn,argnum,retval;
	int64_t i;
	/* tsdumps for the source and destination sides */
	tthdr_t *src = NULL;
	tthdr_t *dst = NULL;
	/* used for organizing timestamp entries by thread number */
	tte_t ***src_thread,***dst_thread;
	/* op counter variables for each qthread */
	int64_t *src_op,*dst_op;
	/* number of operations for each qthread */
	int64_t *src_num_ops,*dst_num_ops;
	/* number of qthreads */
	int32_t nthreads;

	/* get command line options */
	argnum = getoptions(argc, argv);
	fn = MAX(argnum,1);

	/* get the src and dst data structs */
	retval = xdd_getdata(argv[fn],argv[fn+1],&src,&dst);
	if (retval == 0) {
		fprintf(stderr,"xdd_getdata() failed... exiting.\n");
		exit(1);
	}

	/* Get the number of qthreads used */
	nthreads = 0;
	i = src->tt_size-1;
	while (i >= 0 && EOF_OP(src->tte[i].op_type)) {
		nthreads = MAX(nthreads,src->tte[i].qthread_number);
		i--;
	}
	nthreads += 1;

	/* allocate counter arrays to keep track of op number for each thread */
	src_op = malloc(nthreads*sizeof(int64_t));
	dst_op = malloc(nthreads*sizeof(int64_t));
	src_num_ops = malloc(nthreads*sizeof(int64_t));
	dst_num_ops = malloc(nthreads*sizeof(int64_t));
	if (!src_op || !dst_op || !src_num_ops || !dst_num_ops) {
		fprintf(stderr,"malloc() failed on the operation counters in main.\n");
		exit(1);
	}
	for (i = 0; i < nthreads; i++) {
		src_op[i] = 0;
		dst_op[i] = 0;
		src_num_ops[i] = 0;
		dst_num_ops[i] = 0;
	}
	/* count number of ops for each thread */
	for (i = 0; i < src->tt_size; i++) {
		src_num_ops[src->tte[i].qthread_number]++;
		dst_num_ops[dst->tte[i].qthread_number]++;
	}
	/* allocate 2D array of pointers to timestamp entries */
	size_t ptr_size = sizeof(tte_t *);
	src_thread = malloc(nthreads*ptr_size);
	dst_thread = malloc(nthreads*ptr_size);
	if (!src_thread || !dst_thread) {
		fprintf(stderr,"malloc() failed on src_thread or dst_thread in main.\n");
		exit(1);
	}
	for (i = 0; i < nthreads; i++) {
		src_thread[i] = malloc(src_num_ops[i]*ptr_size);
		dst_thread[i] = malloc(dst_num_ops[i]*ptr_size);
		if (!src_thread[i] || !dst_thread[i]) {
			fprintf(stderr,"malloc() failed on src_thread[%ld] or dst_thread[%ld] in main.\n",i,i);
			exit(1);
		}
	}

	/* fill 2D arrays with tte pointers...
	 * after this loop, src_thread[thread] and dst_thread[thread]
	 * will have an array of pointers to their respective tte entries. */
	int32_t src_tnum,dst_tnum;
	for (i = 0; i < src->tt_size; i++) {
		src_tnum = src->tte[i].qthread_number;
		dst_tnum = dst->tte[i].qthread_number;
		src_thread[src_tnum][src_op[src_tnum]] = &(src->tte[i]);
		dst_thread[dst_tnum][dst_op[dst_tnum]] = &(dst->tte[i]);
		src_op[src_tnum]++;
		dst_op[dst_tnum]++;
	}

	/* write all the outfiles */
	write_outfiles(src,dst,src_thread,dst_thread,nthreads,src_num_ops);

	/* free memory */
	free(src_op);
	free(dst_op);
	free(src_num_ops);
	free(dst_num_ops);
	for (i = 0; i < nthreads; i++) {
		free(src_thread[i]);
		free(dst_thread[i]);
	}
	free(src_thread);
	free(dst_thread);
	free(src);
	free(dst);

	return 0;
}




/** \brief write all the outfiles */
void write_outfiles(tthdr_t *src, tthdr_t *dst, tte_t ***src_thread,
		tte_t ***dst_thread, int32_t nthreads, int64_t *num_ops) {

	int64_t i,j;
	/* variables for the file writing loop below */
	FILE *outfile;
	char currfile[OUTFILENAME_LEN];
	char threadstr[6];
	/* number of seconds */
	float read_disk_s,send_net_s,recv_net_s,write_disk_s;

	/* write the outfile_prefix.thread files */
	for (i = 0; i < nthreads; i++) {
		/* concatenate outfile name */
		strncpy(currfile,outfile_prefix,OUTFILENAME_LEN);
		currfile[OUTFILENAME_LEN-1] = '\0';
		sprintf(threadstr,".%04ld",i);
		strncat(currfile,threadstr,OUTFILENAME_LEN);
		currfile[OUTFILENAME_LEN-1] = '\0';
		/* try to open file */
		outfile = fopen(currfile, "w");
		if (outfile == NULL) {
			fprintf(stderr,"Can not open output file: %s\n",currfile);
			continue;
		}

		fprintf(outfile,"#op_number  read_disk  send_net  recv_net  write_disk  units\n");
		for (j = 0; j < num_ops[i]; j++) {
			/* get number of seconds for each op */
			read_disk_s  = pclk2sec((src_thread[i][j]->disk_end -
					src_thread[i][j]->disk_start),src->res);
			send_net_s   = pclk2sec((src_thread[i][j]->net_end -
					src_thread[i][j]->net_start),src->res );
			recv_net_s   = pclk2sec((dst_thread[i][j]->net_end -
					dst_thread[i][j]->net_start),dst->res );
			write_disk_s = pclk2sec((dst_thread[i][j]->disk_end -
					dst_thread[i][j]->disk_start),dst->res);
			fprintf(outfile,"%10ld %10.4f %9.4f %9.4f %11.4f %6s\n",
					src_thread[i][j]->op_number,
					read_disk_s,
					send_net_s,
					recv_net_s,
					write_disk_s,
					"sec");
		}
		fclose(outfile);
	}
}


/*****************************************************
 * \brief Reads file1 and file2, then returns the respective
 * source and destination structures.
 *****************************************************/
int xdd_getdata(char *file1, char *file2, tthdr_t **src, tthdr_t **dst) {

	char *filename = NULL;
	tthdr_t *tsdata = NULL;
	size_t tsdata_size = 0;

	/* read file1 and file2 */
	char i;
	for (i=0; i < 2; i++) {
		if (i == 0)
			filename = file1;
		else if (i == 1)
			filename = file2;

		/* read xdd timestamp dump */
		int retval = xdd_readfile(filename, &tsdata, &tsdata_size);
		if (retval == 0) {
			fprintf(stderr,"Failed to read binary XDD timestamp dump: %s\n",filename);
			return 0;
		}

		/* no empty sets */
		if (tsdata->tt_size < 1) {
			fprintf(stderr,"Timestamp dump was empty: %s\n",filename);
			return 0;
		}

		/* determine whether this is the source or destination file */
		if (READ_OP(tsdata->tte[0].op_type)) {
			if (*src != NULL) {
				fprintf(stderr,"You passed 2 source dump files.\n");
				fprintf(stderr,"Please pass matching source and destination dumps.\n");
				return 0;
			}
			*src = tsdata;
		} else if (WRITE_OP(tsdata->tte[0].op_type)) {
			if (*dst != NULL) {
				fprintf(stderr,"You passed 2 destination dump files.\n");
				fprintf(stderr,"Please pass matching source and destination dumps.\n");
				return 0;
			}
			*dst = tsdata;
		} else {
			fprintf(stderr,"Timestamp dump had invalid operation in tte[0]: %s\n",filename);
			return 0;
		}

		tsdata = NULL;
		filename = NULL;
	}

	/* If these files are from the same transfer,
	* they should be the same size. */
	if ((*src)->tt_bytes != (*dst)->tt_bytes) {
		fprintf(stderr,"Source and destination files are not the same size.\n");
		fprintf(stderr,"Please pass matching source and destination dumps.\n");
		return 0;
	}

	return 1;
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
 *  \return 1 if succeeded, 0 if failed
 *********************************************************/
int xdd_readfile(char *filename, tthdr_t **tsdata, size_t *tsdata_size) {

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


/********************************************
 * getoptions()
 * \brief
 * Parses argument list for options
 *
 * Any arguments in argv that are not
 * recognized here are assumed to be files.
 * This function returns the number of
 * the first non-option argument.  It is up
 * to xdd_readfile() to make sure they
 * are actual filenames.
 ********************************************/
int getoptions(int argc, char **argv) {

	int ierr, opt, argnum;
	extern char *optarg;
	ierr = 0;
	argnum = 1; /* track number of args parsed */

	/* set default options */
	strcpy(outfile_prefix,"(null)");

	/* loop through options */
	while ((opt = getopt(argc, argv, "o:h")) != -1) {
		switch (opt) {
		case 'h': /* help */
			printusage(argv[0]);
			break;
		case 'o': /* output file name */
			strncpy(outfile_prefix,optarg,OUTFILENAME_LEN);
			outfile_prefix[OUTFILENAME_LEN-1] = '\0';
			argnum += 2;
			if (strlen(outfile_prefix) == 0)
				ierr++;
			break;
		default:
			printusage(argv[0]);
			break;
		}
	}

	/* do we have two filenames and no parsing errors? */
	if ( (argc-argnum != 2) || (ierr != 0) ) {
		printusage(argv[0]);
	}

	/* return the number of the first non-option argument */
	return argnum;
}


/** \brief prints some help text and exits */
void printusage(char *progname) {
	fprintf(stderr, "\n");
	fprintf(stderr, "USAGE: %s [OPTIONS] FILE FILE\n\n",progname);

	fprintf(stderr, "This program takes 2 XDD timestamp dump files (source and dest),\n");
	fprintf(stderr, "and writes <file_prefix>.thread files containing the bandwidth\n");
	fprintf(stderr, "of each operation in an xddcp transfer.\n\n");
	fprintf(stderr, "To get an XDD timestamp dump, use the '-ts dump FILE' option of XDD.\n\n");

	fprintf(stderr, "OPTIONS:\n");
	fprintf(stderr, "\t -h              \t print this usage text\n");
	fprintf(stderr, "\t -o <file_prefix>\t output filename prefix\n");

	fprintf(stderr, "\n");
	exit(1);
}


