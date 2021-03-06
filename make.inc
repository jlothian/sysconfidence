# Confidence Makefile

# setup something like the following to use XDD... however, XDD is a moving target and the will require tweaking
# 	USE_XDD = -DUSE_XDD -Ixdd/src/compat -Ixdd/src/base -Ixdd/src/common -Ixdd/src/client -Ixdd/src/net -Ixdd/src/fs
#   **OR** You may try to use some autodetection for XDD includes and FLAGS, such as:
#   USE_XDD = -DUSE_XDD $(shell scripts/get-xdd-flags.sh)
# 	XDD_TARGETS = read_xdd
# 	XDD_OBJS = io_test.o
#       XDD_LIBS  = libxdd.a
# or give up on XDD
# 	USE_XDD = 
# 	XDD_TARGETS =
# 	XDD_OBJS =
# 	XDD_LIBS =
USE_XDD = 
XDD_TARGETS =
XDD_OBJS =
XDD_LIBS =

# USE_SHMEM controls the replacement of MPI calls by SHMEM calls.
# build with MPI ONLY:
#   USE_SHMEM = 
#   SHMEM_LIB = 
# build with SHMEM ONLY:
#   USE_SHMEM = -DSHMEM
#   SHMEM_LIBS = -lsma
# USE_SHMEM =
# SHMEM_LIBS =
# USE_SHMEM = -DSHMEM
# SHMEM_LIBS = -lsma


#
# where are the MPI libraries and which to include?
#    MPI_LIBDIR = -L/usr/lib64/openmpi/1.3.1-gcc
#    MPI_LIBS   = -lmpi
MPI_LIBDIR = 
MPI_LIBS   =

# compiler and flags

CC       = mpicc
# CFLAGS -- use the last one
CFLAGS   = -m64 -march=core2 -mtune=core2 -O3
CFLAGS   = -m64 -O3  $(USE_XDD) $(USE_SHMEM)

# no need for mpi in read_xdd
GCC	 = gcc

# libraries
LIBS = $(MPI_LIBDIR) $(MPI_LIBS) $(SHMEM_LIBS) $(XDD_LIBS) -lpthread -lrt

