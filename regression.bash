#!/bin/bash
#PBS -A STF006
#PBS -N SystemConfidence
#PBS -l size=128
# 12288 for jaguar, 384 for chester
####### size=24
####### mppwidth=32
####### mppnppn=1
####### mppdepth=12
#PBS -l walltime=03:00:00
#PBS -j eo
#PBS

# set -x
#job configuration adjust for test platform

# If MAILREPORT is set to "false", email reporting is disabled. If
# MAILREPORT is set to something other than "false", the script will
# attempt to use the value of MAILREPORT as an email address and send a
# copy fo the Pass/Fail report to that address.  NOTE: The email subject
# will include the hostname.
export MAILREPORT=false

export BUILDDIR=${HOME}/DEVTREE/sysconfidence

export SHORTHOSTNAME=$(hostname --short)
case ${SHORTHOSTNAME} in
	stride*)
		export PROCESSORS_PER_NODE=2
		export CORES_PER_PROCESSOR=8
		export CORES_PER_NODE=16
		export GPUS_PER_NODE=0
		export WORKERS=$(( ${CORES_PER_NODE} + ${GPUS_PER_NODE} ))
		export RANKS=6
		export COMMSIZES="-c1 -c1000 -c5000 -c10000 -c50000"
		export COMMSIZES=""
		export COMMSIZES="-c1 -c10 -c100"
		export MASK1="MASK  0  1  2  3  4  5  6  7"
		export MASK2="MASK  8  9 10 11 12 13 14 15"
		export MASK3=""
		export MASK4=""
		export MASK5=""
		export MASK6=""
		export MASK7=""
		export MASK8=""
		export SYSTEMBURN=systemburn-stride
		export WORKDIR=${BUILDDIR}/regression_test
		export MPI_INCANTATION="mpirun --verbose -np 2 --npernode 1 --hostfile ${HOME}/hostfiles/stride -mca btl tcp,sm,self --prefix /usr/lib64/openmpi -x ENVIRONMENT=BATCH"
		export RUNTIMETAGLINE='XXXXXXXXXXXXXXXXXXXXXXXXXXXXX'
		: ;;
	chester*)
		export PROCESSORS_PER_NODE=1
		export CORES_PER_PROCESSOR=16
		export CORES_PER_NODE=16
		export GPUS_PER_NODE=1
		export GPUTHREADS=512
		export GPULOOPS=16
		export WORKERS=$(( ${CORES_PER_NODE} + ${GPUS_PER_NODE} ))
		export COMMSIZES="1 10"
		export MASK1="MASK 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15"
		export MASK2=""
		export MASK3=""
		export MASK4=""
		export MASK5=""
		export MASK6=""
		export MASK7=""
		export MASK8=""
		export GPU_MASK="MASK 0 4 8 12 "
		export RANKS=32
		export RANKS=$(( ${PBS_NNODES} / ${CORES_PER_NODE} ))
		export RANKS=${PBS_NNODES}
		export SYSTEMDONFIDENCE="sysconfidence-shmem sysconfidence-mpi"
		export WORKDIR=/tmp/work/kuehn
		export MPI_INCANTATION="aprun"
		export MPI_INCANTATION="aprun -n${RANKS} -N1 "
		export MPI_INCANTATION="aprun -n${RANKS}"
		export XT_SYMMETRIC_HEAP_SIZE=256M
		# export SHMEM_ENV_DISPLAY=1
		# export SHMEM_MEMINFO_DISPLAY=1
		export RUNTIMETAGLINE='Application.*resources: utime.*stime.*|Application .* exit codes: .*'
		: ;;
	jaguar*)
		export PROCESSORS_PER_NODE=2
		export CORES_PER_PROCESSOR=6
		export CORES_PER_NODE=12
		export GPUS_PER_NODE=0
		export WORKERS=$(( ${CORES_PER_NODE} + ${GPUS_PER_NODE} ))
		export COMMSIZES="-c1 -c1000 -c60000"
		export MASK1="MASK  0  1  2  3  4  5"
		export MASK2="MASK  6  7  8  9 10 11"
		export MASK3=""
		export MASK4=""
		export MASK5=""
		export MASK6=""
		export MASK7=""
		export MASK8=""
		export RANKS=1024
		export RANKS=$(( ${PBS_NNODES} / ${CORES_PER_NODE} ))
		export SYSTEMBURN=systemburn-jaguar
		export WORKDIR=/tmp/work/kuehn
		export MPI_INCANTATION="aprun -n${RANKS} -N1 -d${CORES_PER_NODE}"
		export RUNTIMETAGLINE='Application.*resources: utime.*stime.*'
		: ;;
	*)
		echo HOSTNAME unrecognized. bailing out...
		exit
esac

JOBTIME=$(date +"%Y%m%d-%H%M%S")
cd ${BUILDDIR}
export WORKDIR="${WORKDIR}/SystemConf.${JOBTIME}"
mkdir -p ${WORKDIR}
cp sysconfidence-shmem sysconfidence-mpi ${WORKDIR}
cd ${WORKDIR}

echo > report

COMMONOPTS="-t bit -C 1000 "
echo $(date +"%Y/%m/%d %H:%M:%S %Z (%s)") " TEST BEGINS" | tee -a report
for TESTOPTS in "-B 8"   "-B 64"   "-B 1024"
do
	for EXE in sysconfidence-mpi sysconfidence-shmem
	do
		OPTIONS="$COMMONOPTS $TESTOPTS"
		OUTNAME=$(echo $EXE $OPTIONS | tr ' ' '_')
		echo $(date +"%Y/%m/%d %H:%M:%S %Z (%s)") "      TEST BEGINS for $EXE $OPTIONS"
		${MPI_INCANTATION} ./${EXE} ${OPTIONS}  > r.${OUTNAME}.stdout 2> r.${OUTNAME}.stderr
		echo $(date +"%Y/%m/%d %H:%M:%S %Z (%s)") "      TEST ENDS for $EXE $OPTIONS"
	done 2>&1
done | tee -a report
echo $(date +"%Y/%m/%d %H:%M:%S %Z (%s)") " TEST ENDS" | tee -a report

exit 0
