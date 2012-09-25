#!/bin/bash -x
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



# A hackish way to call the main() function of
# XDD directly, without having to modify XDD.
#
# This just renames main() to xdd_main(),
# compiles the objects for XDD, and then throws
# the objects into a static library.
#
# If this method works well, we should consider
# making it a configure option in XDD.

XDD_SRC="xdd/src/client/xdd.c"
CC=gcc
TMP=$(mktemp)

# change main() to xdd_main()
sed 's/^main(/xdd_main(/g' < ${XDD_SRC} >xdd_main.c
# go to xdd dir
cd xdd
# configure xdd if needed
grep -q '^configure: exit 0' config.log || ./configure \
    || (echo "XDD configure failed! Please run manually." && cd .. && exit 1)
# make xdd object files and get compile line for xdd_main.c
make clean && rm -f src/xdd_main.o
echo "Running 'make' for XDD and capturing output..."
COMPILEME=$(make | grep "src/client/xdd.o src/client/xdd.c" | sed 's#src/client/xdd.o src/client/xdd.c#../xdd_main.o ../xdd_main.c#g')
echo STEP 1:
echo $COMPILEME
# remove superfluous quoting
COMPILEME=$(echo $COMPILEME | sed 's/\\\"/\"/g' | sed 's/\\ / /g')
# it's much easier to write this to a file than to try to debug multiple levels of weird bash quoting
echo "$COMPILEME" >  ${TMP}
bash -x ${TMP} || (echo "XDD compile failed! Please run make for XDD and compile xdd_main.c manually." && cd .. && exit 1)
rm ${TMP}
# use the static library that xdd builds, and add our new functions
make lib/libxdd.a
cp lib/libxdd.a ..
ar rv ../libxdd.a  ../xdd_main.o 
# go back to sysconfidence dir
cd ..
