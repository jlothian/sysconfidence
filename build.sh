#!/bin/sh

echo ""
echo ""
echo Building the MPI-ONLY version of sysconfidence
echo ""
unset USE_SHMEM
unset SHMEM_LIBS
make clean
make
mv sysconfidence sysconfidence-mpi

echo ""
echo ""
echo Building the SHMEM-ONLY version of sysconfidence
echo ""
export USE_SHMEM=-DSHMEM
export SHMEM_LIBS=-lsma
make clean
make
mv sysconfidence sysconfidence-shmem
