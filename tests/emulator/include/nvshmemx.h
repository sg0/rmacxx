// CPU emulation of the NVSHMEMX subset used by RMACXX (testing only)
#pragma once
#include "nvshmem.h"
#include "mpi.h"

#define NVSHMEMX_INIT_WITH_MPI_COMM 1
struct nvshmemx_init_attr_t
{
    void* mpi_comm = nullptr;
};

inline int nvshmemx_init_status() { return emu::tl_nvshmem_status; }
inline int nvshmemx_init_attr( unsigned, nvshmemx_init_attr_t* )
{
    emu::tl_nvshmem_status = NVSHMEM_STATUS_IS_INITIALIZED;
    emu::barrier();
    return 0;
}

inline void nvshmemx_putmem_nbi_warp( void* d, const void* s, size_t n, int pe ) { nvshmem_putmem( d, s, n, pe ); }
inline void nvshmemx_putmem_nbi_block( void* d, const void* s, size_t n, int pe ) { nvshmem_putmem( d, s, n, pe ); }
inline void nvshmemx_getmem_nbi_warp( void* d, const void* s, size_t n, int pe ) { nvshmem_getmem( d, s, n, pe ); }
inline void nvshmemx_getmem_nbi_block( void* d, const void* s, size_t n, int pe ) { nvshmem_getmem( d, s, n, pe ); }

inline void nvshmemx_quiet_on_stream( cudaStream_t ) {}
inline void nvshmemx_barrier_all_on_stream( cudaStream_t ) { emu::barrier(); }
inline int nvshmemx_buffer_register( void*, size_t ) { return 0; }
inline int nvshmemx_buffer_unregister( void* ) { return 0; }
