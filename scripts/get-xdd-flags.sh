#!/bin/bash
#  This file is part of SystemBurn.
#
#  Copyright (C) 2012, UT-Battelle, LLC.
#
#  This product includes software produced by UT-Battelle, LLC under Contract No. 
#  DE-AC05-00OR22725 with the Department of Energy. 
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the New BSD 3-clause software license (LICENSE). 
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the 
#  LICENSE for more details.
#
#  For more information please contact the SystemBurn developers at: 
#  systemburn-info@googlegroups.com


PATCH0="scripts/xdd.Makefile.patch"

#CFLAGS=$(grep "^DEFS" xdd/config.log | sed "s/DEFS='//" | sed "s/'//")

# only ptch if we need to
if ! grep -q print_includes xdd/Makefile ; then
  patch -p0 < ${PATCH0} > /dev/null
fi
INCLUDES=$( cd xdd ; make print_includes | sed 's/I/Ixdd\//g')
CPPFLAGS=$( cd xdd ; make print_cppflags)
echo ${INCLUDES} ${CPPFLAGS}
