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


#ifndef _TYPES_H
#define _TYPES_H

#include <inttypes.h>


/**************************************************************
 * LABEL_LEN    -- length of labels in structs below
 * FNAMESIZE    -- measurement outfile filenames length
 * NAMEBUFFSIZE -- POSIX and OpenMPI both require at least 256
 *                 while UNIX requires 8-14
 * ROOTONLY     -- readabililty macro for that serial stuff
 **************************************************************/
#define LABEL_LEN 64
#define FNAMESIZE 512
#define NAMEBUFFSIZE 256
#define NODIVIDEBYZERO(_N_) ( (_N_ == 0) ? (1) : (_N_))


/**************************************************************
 * TYPES
 **************************************************************/

/* generic communication buffer */
typedef struct buffer {
	void *data;		/* buffer data */
	size_t len;		/* buffer length in bytes */
} buffer_t;

/* holds distribution and stats for timings */
typedef struct histogram {
	char label[LABEL_LEN];	/* label */
	double min0;		/* minimum (mins=1) */
	double mod0, mods;	/* mode: 0,scaled */
	double med0, meds;	/* median: 0,scaled */
	double max0, maxs;	/* maximum: 0,scaled */
	double m10, m1m, m1s;	/* 1st moment: 0,min,scaled */
	double m20, m2m, m2s;	/* 2nd moment: 0,min,scaled */
	double m30, m3m, m3s;	/* 3rd moment: 0,min,scaled */
	double m40, m4m, m4s;	/* 4th moment: 0,min,scaled */
	uint64_t nsamples;	/* number of samples in the distribution */
	uint64_t *dist;		/* pointer to histogram array: dist[nbins] */
} histogram_t;

/* group histograms together */
typedef struct measurement {
	char label[LABEL_LEN];	/* label */
	int nbins;		/* number of bins in the histograms */
	int buflen;		/* message size in bytes */
	double binwidth;	/* bin interval : [n*binwidth,(n+1)*binwidth] */
	double timer_oh;	/* timer overhead in seconds */
	int num_histograms;	/* number of histograms in the array */
	histogram_t *hist;	/* array of histograms for this measurement */
} measurement_t;

/* config options for each test */
typedef struct test {
	/* name for this test */
	char case_name[NAMEBUFFSIZE];
	/* which test we're running */
	int test_type;
	/* define how to run through the test*/
	int num_stages;
	int num_warmup;         /* keep this < 1% of num_messages */
	int num_messages;       /* messages per cycle*/
	int num_cycles;         /* how many times to cycle through the test */
	/* uint64_t total_messages; */ /* num_cycles * num_messages * (num_ranks-1) */
	/* histogram options */
	int num_bins;           /* number of bins */
	double bin_size;        /* size of bin in seconds */
	double max_hist_time;   /* max histogram time in seconds */
	double hist_scale;      /* histogram scale in seconds */
	/* message size */
	int buf_len;
	/* misc options */
	char log_binning;       /* logarithmic binning (yes/no) */
	char rank_mapping;      /* whether to output rank mapping */
	/* arguments to pass to io test */
	int argc;
	char **argv;
	/* filename prefix passed to the XDD '-ts dump' option
	 * this will result in a file named tsdump.TSDUMP_POSTFIX */
	char *tsdump;
} test_t;

/* pointer types */
typedef buffer_t* buffer_p;
typedef histogram_t* histogram_p;
typedef measurement_t* measurement_p;
typedef test_t* test_p;


#endif /* _TYPES_H */
