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


#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "copyright.h"
#include "config.h"
#include "types.h"
#include "comm.h"
#include "measurement.h"
#include "options.h"
#include "orbtimer.h"
#include "tests.h"
#ifdef USE_XDD
#  include "xdd_main.h"
#endif

int main(int argc, char *argv[]) {
	measurement_p l,g;
	test_p tst = (test_p)malloc(sizeof(test_t));

	/* initialize communication and get options */
	comm_initialize(tst, &argc, &argv);
	ROOTONLY printf("%s\n", COPYRIGHT);
	general_options(tst,argc,argv);

	/* create measurement structs */
	l = measurement_create(tst, "local");
	g = measurement_create(tst, "global");
	ROOTONLY mkdir(tst->case_name, 0755);

	ROOTONLY printf("Confidence: testing...\n");
	measurement_collect(tst, l);
	ROOTONLY printf("Confidence: local analysis...\n");
	measurement_analyze(tst, l, -1.0);
	ROOTONLY printf("Confidence: remote analysis\n");
	comm_aggregate(g, l);
	measurement_analyze(tst, g, -1.0);
	ROOTONLY printf("Confidence: saving results\n");
	measurement_serialize(tst, g, root_rank);

	/* free measurement and test structs */
	l = measurement_destroy(l);
	g = measurement_destroy(g);
	if (tst->argv != NULL) 
		free(tst->argv);
	if (tst->tsdump != NULL)
		free(tst->tsdump);
	free(tst);

	comm_finalize();
	return 0;
}

