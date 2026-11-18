// ------------------------------------------------------------------------
// CPU emulation of the CUDA runtime / NVSHMEM / MPI subset used by RMACXX.
// ------------------------------------------------------------------------
// TESTING ONLY. Each PE is a std::thread of one process; the symmetric heap
// is one arena per PE, and NVSHMEM calls translate symmetric addresses
// between arenas. Kernels run serially on the calling host thread (blocks
// and threads are iterated, cooperative warp/block NVSHMEM calls are
// executed by every emulated thread, which is idempotent for put/get).
// AMOs are serialized with a global mutex. Everything is synchronous, so
// quiet/fence are no-ops.
//
// This lets the RMACXX test-suite (index arithmetic, patch decomposition,
// staging, expression machinery, AMO emulation) run on machines without
// GPUs, e.g. in CI. It says nothing about GPU performance or memory-model
// subtleties.
// ------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define __host__
#define __device__
#define __global__
#define __forceinline__ inline

#define RMACXX_TLS thread_local

struct dim3
{
    unsigned x = 1, y = 1, z = 1;
    dim3() = default;
    dim3( unsigned a, unsigned b = 1, unsigned c = 1 ) : x( a ), y( b ), z( c ) {}
};

namespace emu
{
inline thread_local dim3 tl_threadIdx, tl_blockIdx, tl_blockDim, tl_gridDim;

struct World
{
    int npes = 1;
    size_t arena_bytes = size_t( 256 ) << 20;
    std::vector<char*> arena;
    std::unique_ptr<std::barrier<>> bar;
    std::mutex amo_mtx;
    double t0 = 0;
};

inline World& world()
{
    static World w;
    return w;
}

inline thread_local int tl_pe = 0;
inline thread_local size_t tl_heap_off = 0;
inline thread_local bool tl_finalized = false;

inline void barrier() { world().bar->arrive_and_wait(); }

// translate a symmetric address of the calling PE into PE `pe`'s arena
inline char* xlate( const void* sym, int pe )
{
    World& w = world();
    const char* p = static_cast<const char*>( sym );
    const char* mine = w.arena[tl_pe];

    if ( p < mine || p >= mine + w.arena_bytes )
    {
        std::fprintf( stderr, "[emu] PE %d: address %p is not symmetric\n", tl_pe, sym );
        std::abort();
    }

    if ( pe < 0 || pe >= w.npes )
    {
        std::fprintf( stderr, "[emu] PE %d: invalid target PE %d\n", tl_pe, pe );
        std::abort();
    }

    return w.arena[pe] + ( p - mine );
}

// threads per emulated block are capped to keep cooperative calls cheap
inline constexpr unsigned kMaxEmuThreads = 4;

template <typename F>
inline void launch( dim3 grid, dim3 block, F&& f )
{
    block.x = block.x > kMaxEmuThreads ? kMaxEmuThreads : block.x;
    grid.x = grid.x > 8 ? 8 : grid.x;
    tl_gridDim = grid;
    tl_blockDim = block;

    for ( unsigned b = 0; b < grid.x; b++ )
        for ( unsigned t = 0; t < block.x; t++ )
        {
            tl_blockIdx = dim3( b, 0, 0 );
            tl_threadIdx = dim3( t, 0, 0 );
            f();
        }
}

// run `body` on npes threads, one per PE
inline int run( int npes, std::function<int( int )> body )
{
    World& w = world();
    w.npes = npes;
    w.bar = std::make_unique<std::barrier<>>( npes );
    w.arena.resize( npes );

    for ( int p = 0; p < npes; p++ )
    {
        w.arena[p] = static_cast<char*>( std::calloc( 1, w.arena_bytes ) );

        if ( !w.arena[p] )
        {
            std::fprintf( stderr, "[emu] cannot allocate arena\n" );
            return 2;
        }
    }

    w.t0 = std::chrono::duration<double>( std::chrono::steady_clock::now().time_since_epoch() ).count();
    std::vector<int> rc( npes, 0 );
    std::vector<std::thread> th;

    for ( int p = 0; p < npes; p++ )
        th.emplace_back( [&, p] { tl_pe = p; rc[p] = body( p ); } );

    for ( auto& t : th ) t.join();

    int r = 0;
    for ( int v : rc ) r |= v;
    return r;
}
} // namespace emu

#define threadIdx ( ::emu::tl_threadIdx )
#define blockIdx ( ::emu::tl_blockIdx )
#define blockDim ( ::emu::tl_blockDim )
#define gridDim ( ::emu::tl_gridDim )
constexpr int warpSize = 32;

inline void __syncthreads() {}
inline void __syncwarp( unsigned = 0xffffffffu ) {}
inline void __threadfence_system() {}

#define RMACXX_LAUNCH( kern, grid, block, stream, ... ) \
    ::emu::launch( dim3( grid ), dim3( block ), [&]() { kern( __VA_ARGS__ ); } )
