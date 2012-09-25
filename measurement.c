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
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <sys/stat.h>

#include "measurement.h"
#include "copyright.h"
#include "comm.h"
#include "tests.h"

/**********************************************
 * \brief Convert a time (in seconds) to a bin number
 **********************************************/
inline int time2bin(test_p tst, double t) {
	int b;
	if (t < tst->max_hist_time) {
		/* LINEAR binning */
		if (tst->log_binning == 0) {
			b = (int)(t / tst->bin_size);
		/* LOGARITHMIC BINNING */
		} else {
			b = (int)(tst->hist_scale * log(t / tst->bin_size));
			/* drop the small ones in the zeroeth bin */
			b = ((b > 0) ? b : 0);
		}
	} else {
		/* drop the big ones in the last bin */
		b = tst->num_bins - 1;
	}
	return b;
}

/**************************************************
 * \brief Convert a bin to it's corresponding bottom time
 **************************************************/
inline double bin2time(test_p tst, int b) {
	double t;
	/* first bin is special... goes to zero */
	if (b == 0) return 0.0;
	/* LINEAR binning */
	if (tst->log_binning == 0) {
		t = tst->bin_size * ((double)b);
	/* LOGARITHMIC binning */
	} else {
		t = tst->bin_size * exp((((double)b) / tst->hist_scale));
	}
	return t;
}

/***************************************************
 * \brief Convert a bin to it's corresponding midpoint time
 ***************************************************/
inline double bin2midtime(test_p tst, int b) {
	double t;
	/* LINEAR binning */
	if (tst->log_binning == 0) {
		t = tst->bin_size * ((double)b + 0.5);
	/* LOGARITHMIC binning */
	} else {
		t = tst->bin_size * exp((((double)b + 0.5) / tst->hist_scale));
	}
	return t;
}


/** \brief main() calls measurement_create()
 * measurement_create() calls net_measurement_create() (assuming tst->test_type == 1)
 * net_measurement_create() uses measurement_real_create() */
measurement_p measurement_create(test_p tst, char *label) {
	switch (tst->test_type) {
		default:
		case NET_TEST:		return net_measurement_create(tst, label);
		case BIT_TEST:		return bit_measurement_create(tst, label);
#ifdef USE_XDD
		case IO_TEST:		return io_measurement_create(tst, label);
#endif
	}
}

/**********************************************
 * \brief constructor routine for MEASUREMENT
 **********************************************/
measurement_p measurement_real_create(test_p tst, char *label, int histograms) {
	int i;

	/* create a container to hold a collected dataset */
	measurement_p m = (measurement_p)malloc(sizeof(measurement_t));
	assert(m != NULL);

	/* allocate memory for the histograms */
	m->hist = (histogram_p)malloc(sizeof(histogram_t)*histograms);
	assert(m->hist != NULL);

	/* alloc distribution arrays */
	for (i = 0; i < histograms; i++) {
		m->hist[i].dist = comm_alloc_dist((size_t)tst->num_bins);
		assert(m->hist[i].dist != NULL);
	}

	/* copy vars */
	m->num_histograms = histograms;
	m->nbins = tst->num_bins;
	m->binwidth = tst->bin_size;
	m->buflen = tst->buf_len;
	strncpy(m->label, label, LABEL_LEN);

	return m;
}

/**********************************************
 * \brief Destructor routine for MEASUREMENT
 **********************************************/
measurement_p measurement_destroy(measurement_p m) {
	int i;
	for (i = 0; i < m->num_histograms; i++) {
		comm_free_dist(m->hist[i].dist);
	}
	free(m->hist);
	free(m);
	return NULL;
}

/**********************************************
 * \brief Select test based on tst->test_type
 **********************************************/
void measurement_collect(test_p tst, measurement_p m) {
	switch (tst->test_type) {
		case NET_TEST:
			net_test(tst, m);
			break;
		case BIT_TEST:
			bit_test(tst, m);
			break;
#ifdef USE_XDD
		case IO_TEST:
			io_test(tst, m);
			break;
#endif
		default:
			ROOTONLY fprintf(stderr, "No test was specified.\n");
			break;
	}
}

/**********************************************
 * \brief Compute the moments for a histogram
 **********************************************/
void measurement_moments(test_p tst, histogram_p h, double center, double *m1, double *m2, double *m3, double *m4) {
	/* calculate four moments of a distribution about a particular bin, given nbins, and binwidth */
	int i;
	double x;
	*m1 = 0.0;
	*m2 = 0.0;
	*m3 = 0.0;
	*m4 = 0.0;
	if (h->nsamples != 0) {
		for (i = 0; i < tst->num_bins; i++) {
			x = bin2midtime(tst,i) - center;
			*m1 += (h->dist)[i] * x;
			*m2 += (h->dist)[i] * x * x;
			*m3 += (h->dist)[i] * x * x * x;
			*m4 += (h->dist)[i] * x * x * x * x;
		}
		*m1 /= h->nsamples;
		*m2 /= h->nsamples;
		*m3 /= h->nsamples;
		*m4 /= h->nsamples;
	}
	return;
}

/**********************************************
 * \brief Count the samples in a histogram
 **********************************************/
uint64_t measurement_samplecount(uint64_t *dist, int nbins) {
	int i;
	uint64_t nsamples = 0;
	for (i = 0; i < nbins; i++) {
		nsamples += dist[i];
	}
	return nsamples;
}

/**********************************************
 * \brief Compute the statistics on a histogram
 **********************************************/
void measurement_histogram(test_p tst, histogram_p h, double scale) {
	/* compute stats for an individual histogram */
	double s;
	int i, j;
	uint64_t tmp = 0;
	h->nsamples = measurement_samplecount(h->dist, tst->num_bins); /* samples */
	i = -1;
	while ((h->dist)[++i] == 0) ;	/* minimum */
	h->min0 = bin2midtime(tst,i);
	j = 0;
	for (i = 0; i < tst->num_bins; i++)
		if ((h->dist)[i] > (h->dist)[j])
			j = i;		/* mode */
	h->mod0 = bin2midtime(tst,j);
	i = -1;
	while ((tmp += (h->dist)[++i]) < (h->nsamples) / 2) ;	/* median */
	h->med0 = bin2midtime(tst,i);
	i = tst->num_bins;
	while ((h->dist)[--i] == 0) ;	/* maximum */
	h->max0 = bin2midtime(tst,i);
	/* compute moments */
	measurement_moments(tst, h, 0.0, &(h->m10), &(h->m20), &(h->m30), &(h->m40));
	measurement_moments(tst, h, (h->min0), &(h->m1m), &(h->m2m), &(h->m3m), &(h->m4m));
	if (scale <= 0.0) {
		s = (h->min0);
	} else {
		s = scale;
	}
	h->mods = (h->mod0) / s;
	h->meds = (h->med0) / s;
	h->maxs = (h->max0) / s;
	h->m1s = (h->m10) / s;	/* 1st moment: scaled */
	h->m2s = (h->m20) / s / s;	/* 2nd moment: scaled */
	h->m3s = (h->m30) / s / s / s;	/* 3rd moment: scaled */
	h->m4s = (h->m40) / s / s / s / s;	/* 4th moment: scaled */
}

/**********************************************
 * \brief Call the analysis routines on each histogram
 **********************************************/
void measurement_analyze(test_p tst, measurement_p m, double scale) {
	int i;
	/* analyze raw measurement data with summary statistics */
	for (i = 0; i < m->num_histograms; i++) {
		measurement_histogram(tst, &(m->hist[i]), scale);
	}
}

/**********************************************
 * \brief Save the data to disk
 **********************************************/
void measurement_serialize(test_p tst, measurement_p m, int writingRankID) {
	/* if I am the target, then I will output the raw data and statistics for visualization */
	/* have to switch from binwidth based descriptions here... */
	if (my_rank == writingRankID) {
//		mkdir(tst->case_name, 0755);
		/* raw histogram data */
		measurement_write_hist(tst, m);
		/* PDF data */
		measurement_write_pdf(tst, m);
		/* CDF data */
		measurement_write_cdf(tst, m);

		int i;
		/* Statistical Summaries */
		for (i = 0; i < m->num_histograms; i++ ) {
			measurement_fmthist(tst, &(m->hist[i]), m->label);
		}

		comm_showmapping(tst);
	}
}

/**
 \brief Write the data into a cdf file
*/
void measurement_write_cdf(test_p tst, measurement_p m) {
	int i,j;
	char fname[FNAMESIZE];
	double *cdf;
	double binwidth, binbot, bintop;
	FILE *Fcdf;
	snprintf(fname, FNAMESIZE, "%s/%s.CDF.%d", tst->case_name, m->label, my_rank);

	size_t cdf_size = m->num_histograms*sizeof(double);
	cdf = (double *)malloc(cdf_size);
	assert(cdf != NULL);
	memset(cdf,0,cdf_size);

	Fcdf = fopen(fname, "w");
	assert(Fcdf != NULL);
	measurement_print_header(Fcdf, tst, m->label, NULL);

	/* print histogram labels */
	fprintf(Fcdf, "#%6s %18s ", "bin", " (us) to  (us)");
	for (j = 0; j < m->num_histograms; j++) {
		fprintf(Fcdf, "%15s ", m->hist[j].label);
	}
	fprintf(Fcdf, "\n");

	/* print the values */
	for (i = 0; i < m->nbins; i++) {
		binbot = bin2time(tst,i);
		bintop = bin2time(tst,(i + 1));
		binwidth = bintop - binbot;
		fprintf(Fcdf, "%6d %11.4g %11.4g ", i, binbot * 1.0e+6, bintop * 1.0e+6);
		for (j = 0; j < m->num_histograms; j++) {
			cdf[j] += (double)(m->hist[j].dist[i]) / (double)NODIVIDEBYZERO(m->hist[j].nsamples);
			fprintf(Fcdf, "%15.8e ", cdf[j]);
		}
		fprintf(Fcdf, "\n");
	}

	fclose(Fcdf);
	free(cdf);
}

/**
 \brief Write the data into a pdf file
*/
void measurement_write_pdf(test_p tst, measurement_p m) {
	int i,j;
	char fname[FNAMESIZE];
	double binwidth, binbot, bintop;
	FILE *Fpdf;
	snprintf(fname, FNAMESIZE, "%s/%s.PDF.%d", tst->case_name, m->label, my_rank);

	Fpdf = fopen(fname, "w");
	assert(Fpdf != NULL);
	measurement_print_header(Fpdf, tst, m->label, NULL);

	/* print histogram labels */
	fprintf(Fpdf, "#%6s %18s ", "bin", " (us) to  (us)");
	for (j = 0; j < m->num_histograms; j++) {
		fprintf(Fpdf, "%15s ", m->hist[j].label);
	}
	fprintf(Fpdf, "\n");

	/* print the values */
	for (i = 0; i < m->nbins; i++) {
		binbot = bin2time(tst,i);
		bintop = bin2time(tst,(i + 1));
		binwidth = bintop - binbot;
		fprintf(Fpdf, "%6d %11.4g %11.4g ", i, binbot * 1.0e+6, bintop * 1.0e+6);
		for (j = 0; j < m->num_histograms; j++) {
			fprintf(Fpdf, "%15.8e ", (double)(m->hist[j].dist[i]) / binwidth
					/ (double)NODIVIDEBYZERO(m->hist[j].nsamples) );
		}
		fprintf(Fpdf, "\n");
	}

	fclose(Fpdf);
}

/**
 \brief Print out the histogram results of the test
*/
void measurement_write_hist(test_p tst, measurement_p m) {
	int i,j;
	char fname[FNAMESIZE];
	double binwidth, binbot, bintop;
	FILE *Fhist;
	snprintf(fname, FNAMESIZE, "%s/%s.HIST.%d", tst->case_name, m->label, my_rank);

	Fhist = fopen(fname, "w");
	assert(Fhist != NULL);
	measurement_print_header(Fhist, tst, m->label, NULL);

	/* print histogram labels */
	fprintf(Fhist, "#%6s %18s ", "bin", " (us) to  (us)");
	for (j = 0; j < m->num_histograms; j++) {
		fprintf(Fhist, "%15s ", m->hist[j].label);
	}
	fprintf(Fhist, "\n");

	/* print the values */
	for (i = 0; i < m->nbins; i++) {
		binbot = bin2time(tst,i);
		bintop = bin2time(tst,(i + 1));
		binwidth = bintop - binbot;
		fprintf(Fhist, "%6d %11.4g %11.4g ", i, binbot * 1.0e+6, bintop * 1.0e+6);
		for (j = 0; j < m->num_histograms; j++) {
			fprintf(Fhist, "%15"PRIu64" ", m->hist[j].dist[i] );
		}
		fprintf(Fhist, "\n");
	}
	fclose(Fhist);
}


/**********************************************
 * \brief Save the histogram summary stats to disk
 **********************************************/
void measurement_fmthist(test_p tst, histogram_p h, char *label) {
	char fname[FNAMESIZE];
	FILE *Fstat;
	snprintf(fname, FNAMESIZE, "%s/%s.STAT.%s.%d", tst->case_name, label, h->label, my_rank);

	Fstat = fopen(fname, "w");
	assert(Fstat != NULL);
	measurement_print_header(Fstat, tst, label, h->label);

	fprintf(Fstat, "# Number of Samples: %"PRIu64" in %d bins\n", h->nsamples, tst->num_bins);	/* number of samples in the distribution */
	fprintf(Fstat, "\n");
	fprintf(Fstat, "Minimum:      %15.2g usec     %15.2g * minLatency\n", h->min0 * 1.0e+6, 1.0);			/* minimum: 0, 0-scaled */
	fprintf(Fstat, "Mode:         %15.2g usec     %15.2g * minLatency\n", h->mod0 * 1.0e+6, h->mods);		/* mode: 0, 0-scaled */
	fprintf(Fstat, "Median:       %15.2g usec     %15.2g * minLatency\n", h->med0 * 1.0e+6, h->meds);		/* median: 0, 0-scaled */
	fprintf(Fstat, "Mean:         %15.2g usec     %15.2g * minLatency\n", h->m10 * 1.0e+6, h->m10 / h->min0);	/* 1st moment: 0, 0-scaled */
	fprintf(Fstat, "Maximum:      %15.2g usec     %15.2g * minLatency\n", h->max0 * 1.0e+6, h->maxs);		/* maximum: 0, 0-scaled */
	fprintf(Fstat, "\n");
	fprintf(Fstat, "R1(Mean):     %15.2g usec     %15.2g * minLatency\n", h->m10 * 1.0e+6, h->m1s);			/* 1st moment: 0,min-scaled */
	fprintf(Fstat, "R2(Variance): %15.2g usec     %15.2g * minLatency\n", sqrt(h->m20) * 1.0e+6, sqrt(h->m2s));	/* 2nd moment: 0,min-scaled */
	fprintf(Fstat, "R3(Skewness): %15.2g usec     %15.2g * minLatency\n", cbrt(h->m30) * 1.0e+6, cbrt(h->m3s));	/* 3rd moment: 0,min-scaled */
	fprintf(Fstat, "R4(Kurtosis): %15.2g usec     %15.2g * minLatency\n", sqrt(sqrt(h->m40)) * 1.0e+6, sqrt(sqrt(h->m4s)));	/* 4th moment: 0,min-scaled */
	fprintf(Fstat, "\n");
	fprintf(Fstat, "Mean:         %15.2g seconds     %15.2g * minLatency\n", h->m10, h->m1s);			/* 1st moment: 0,min-scaled */
	fprintf(Fstat, "Variance:     %15.2g seconds**2  %15.2g * minLatency**2\n", h->m20, h->m2s);			/* 2nd moment: 0,min-scaled */
	fprintf(Fstat, "Skewness:     %15.2g seconds**3  %15.2g * minLatency**3\n", h->m30, h->m3s);			/* 3rd moment: 0,min-scaled */
	fprintf(Fstat, "Kurtosis:     %15.2g seconds**4  %15.2g * minLatency**4\n", h->m40, h->m4s);			/* 4th moment: 0,min-scaled */
	fclose(Fstat);
}

/**
 \brief Writes the header with the configuration information
*/
void measurement_print_header(FILE *outfile, test_p tst, char *measurement_label, char *hist_label) {
	int i;
	fprintf(outfile, "%s",COPYRIGHT);
	fprintf(outfile, "# Casename:          %s\n", tst->case_name);
	fprintf(outfile, "# TestType:          %d\n", tst->test_type);
	fprintf(outfile, "# NumRanks:          %d\n", num_ranks);
	if (measurement_label != NULL)
		fprintf(outfile, "# MLabel:            %s\n", measurement_label);
	if (hist_label != NULL)
		fprintf(outfile, "# HLabel:            %s\n", hist_label);
	if (tst->test_type == IO_TEST) {
		fprintf(outfile, "# Operation Size:    %d\n", tst->buf_len);
		fprintf(outfile, "# Operation Pattern: %d cycle(s) of %d operations per node\n", tst->num_cycles, tst->num_messages);
		/* print xdd args */
		fprintf(outfile, "# XDD Arguments:     ");
		for (i=1; i<tst->argc; i++)
			fprintf(outfile, "%s ", tst->argv[i]);
		fprintf(outfile, "\n");
	} else {
		fprintf(outfile, "# Message Size:      %d\n", tst->buf_len);
		fprintf(outfile, "# Message Pattern:   %d cycle(s) through an all-pairs schedule\n", tst->num_cycles);
		fprintf(outfile, "#                    of %d warmups and %d messages per pair\n", tst->num_warmup, tst->num_messages);
	}
	if (tst->log_binning == 1) {
		fprintf(outfile, "# Binning:           Logarithmic, ending at %g seconds\n", tst->max_hist_time);
	} else {
		fprintf(outfile, "# Binning:           Linear, ending at %g seconds\n", tst->max_hist_time);
	}
}


