// CPU emulation of the NVSHMEM subset used by RMACXX (testing only)
#pragma once
#include "cuda_runtime.h"

typedef int nvshmem_team_t;
#define NVSHMEM_TEAM_WORLD 0
#define NVSHMEMX_TEAM_NODE 1

enum
{
    NVSHMEM_STATUS_NOT_INITIALIZED = 0,
    NVSHMEM_STATUS_IS_BOOTSTRAPPED,
    NVSHMEM_STATUS_IS_INITIALIZED
};

namespace emu
{
inline thread_local int tl_nvshmem_status = NVSHMEM_STATUS_NOT_INITIALIZED;
}

inline int nvshmem_my_pe() { return emu::tl_pe; }
inline int nvshmem_n_pes() { return emu::world().npes; }
inline int nvshmem_team_my_pe( nvshmem_team_t ) { return emu::tl_pe; }
inline void nvshmem_init() { emu::tl_nvshmem_status = NVSHMEM_STATUS_IS_INITIALIZED; emu::barrier(); }
inline void nvshmem_finalize() { emu::barrier(); emu::tl_nvshmem_status = NVSHMEM_STATUS_NOT_INITIALIZED; }
inline void nvshmem_global_exit( int status ) { std::fflush( stdout ); std::_Exit( status ); }
inline void nvshmem_barrier_all() { emu::barrier(); }
inline void nvshmem_quiet() {}
inline void nvshmem_fence() {}

// symmetric heap: bump allocator per PE, identical sequence on all PEs
inline void* nvshmem_malloc( size_t bytes )
{
    emu::barrier();
    size_t off = emu::tl_heap_off;
    emu::tl_heap_off += ( bytes + 255 ) & ~size_t( 255 );

    if ( emu::tl_heap_off > emu::world().arena_bytes )
        return nullptr;

    return emu::world().arena[emu::tl_pe] + off;
}
inline void nvshmem_free( void* ) { emu::barrier(); }

inline void* nvshmem_ptr( const void* p, int pe ) { return emu::xlate( p, pe ); }

// RMA
inline void nvshmem_putmem( void* d, const void* s, size_t n, int pe ) { std::memmove( emu::xlate( d, pe ), s, n ); }
inline void nvshmem_getmem( void* d, const void* s, size_t n, int pe ) { std::memmove( d, emu::xlate( s, pe ), n ); }
inline void nvshmem_putmem_nbi( void* d, const void* s, size_t n, int pe ) { nvshmem_putmem( d, s, n, pe ); }
inline void nvshmem_getmem_nbi( void* d, const void* s, size_t n, int pe ) { nvshmem_getmem( d, s, n, pe ); }

#define EMU_P_G( NAME, TYPE ) \
    inline void nvshmem_##NAME##_p( TYPE* d, TYPE v, int pe ) { std::memcpy( emu::xlate( d, pe ), &v, sizeof( TYPE ) ); } \
    inline TYPE nvshmem_##NAME##_g( const TYPE* s, int pe ) { TYPE v; std::memcpy( &v, emu::xlate( s, pe ), sizeof( TYPE ) ); return v; }
EMU_P_G( uint8, uint8_t )
EMU_P_G( uint16, uint16_t )
EMU_P_G( uint32, uint32_t )
EMU_P_G( uint64, uint64_t )
#undef EMU_P_G

// AMOs (serialized)
#define EMU_AMO( NAME, TYPE ) \
    inline TYPE nvshmem_##NAME##_atomic_fetch( const TYPE* s, int pe ) { std::lock_guard<std::mutex> g( emu::world().amo_mtx ); TYPE v; std::memcpy( &v, emu::xlate( s, pe ), sizeof( TYPE ) ); return v; } \
    inline void nvshmem_##NAME##_atomic_set( TYPE* d, TYPE v, int pe ) { std::lock_guard<std::mutex> g( emu::world().amo_mtx ); std::memcpy( emu::xlate( d, pe ), &v, sizeof( TYPE ) ); } \
    inline TYPE nvshmem_##NAME##_atomic_swap( TYPE* d, TYPE v, int pe ) { std::lock_guard<std::mutex> g( emu::world().amo_mtx ); TYPE* r = reinterpret_cast<TYPE*>( emu::xlate( d, pe ) ); TYPE o = *r; *r = v; return o; } \
    inline TYPE nvshmem_##NAME##_atomic_compare_swap( TYPE* d, TYPE c, TYPE v, int pe ) { std::lock_guard<std::mutex> g( emu::world().amo_mtx ); TYPE* r = reinterpret_cast<TYPE*>( emu::xlate( d, pe ) ); TYPE o = *r; if ( o == c ) *r = v; return o; } \
    inline TYPE nvshmem_##NAME##_atomic_fetch_add( TYPE* d, TYPE v, int pe ) { std::lock_guard<std::mutex> g( emu::world().amo_mtx ); TYPE* r = reinterpret_cast<TYPE*>( emu::xlate( d, pe ) ); TYPE o = *r; *r = (TYPE)( (unsigned long long)o + (unsigned long long)v ); return o; } \
    inline void nvshmem_##NAME##_atomic_add( TYPE* d, TYPE v, int pe ) { (void)nvshmem_##NAME##_atomic_fetch_add( d, v, pe ); }
EMU_AMO( int, int )
EMU_AMO( longlong, long long )
#undef EMU_AMO
