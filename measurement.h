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


#ifndef _MEASUREMENT_H
#define _MEASUREMENT_H

#include "types.h"

/**************************************************************
 * FUNCTIONS
 **************************************************************/
extern inline int time2bin(test_p tst, double t);
extern inline double bin2time(test_p tst, int b);
extern inline double bin2midtime(test_p tst, int b);

/* measurement constructor and destructor */
measurement_p measurement_create(test_p tst, char *label);
measurement_p measurement_real_create(test_p tst, char *label, int histograms);
measurement_p measurement_destroy(measurement_p m);

/* calls the test specified in tst->test_type */
void measurement_collect(test_p tst, measurement_p m);

/* analysis functions */
void measurement_moments(test_p tst, histogram_p h, double center, double *m1, double *m2, double *m3, double *m4);
uint64_t measurement_samplecount(uint64_t *dist, int nbins);
void measurement_histogram(test_p tst, histogram_p h, double scale);
void measurement_analyze(test_p tst, measurement_p m, double scale);

/* output functions */
void measurement_serialize(test_p tst, measurement_p m, int writingRankID);
void measurement_write_cdf(test_p tst, measurement_p m);
void measurement_write_pdf(test_p tst, measurement_p m);
void measurement_write_hist(test_p tst, measurement_p m);
void measurement_fmthist(test_p tst, histogram_p h, char *label);
void measurement_print_header(FILE *outfile, test_p tst, char *measurement_label, char *hist_label);


#endif /* _MEASUREMENT_H */

