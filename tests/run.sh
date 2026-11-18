#!/usr/bin/env bash
# Build and run a single test on GPUs with NVSHMEM (MPI bootstrap).
# Usage (from the repository root): tests/run.sh tests/expr-elem/expr-elem 2
# Requires NVSHMEM_HOME (and CUDA_HOME, default /usr/local/cuda); set ARCH
# (default sm_80) to your GPU architecture.
# For the CPU emulator use tests/emulator/run_tests.sh tests/expr-elem/expr-elem.cpp
NVSHMEM_HOME=${NVSHMEM_HOME:?set NVSHMEM_HOME}
CUDA_HOME=${CUDA_HOME:-/usr/local/cuda}
ARCH=${ARCH:-sm_80}
nvcc -x cu -rdc=true -ccbin mpicxx -std=c++20 -O3 -arch=$ARCH -I./include -I$NVSHMEM_HOME/include \
     $1.cpp -o $1 -L$NVSHMEM_HOME/lib -lnvshmem_host -lnvshmem_device -lcuda &&
mpiexec -np $2 $1 &&
rm $1;
