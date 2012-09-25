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


#ifndef HAVE_OPTIONS_H
#define HAVE_OPTIONS_H

#include "types.h"

/**************************************************************
 * FUNCTION PROTOTYPES
 **************************************************************/
/* set default arg values */
void setdefaults(test_p tst);
/* argument parsers */
void general_options(test_p tst, int argc, char *argv[]);
void parse_xdd_args(test_p tst, char *optarg, char *progname);
/* print help text */
void print_help(test_p tst, char *progname);

#endif				/* HAVE_OPTIONS_H */

