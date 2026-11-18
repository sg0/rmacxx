// ------------------------------------------------------------------------
// RMACXX NVSHMEM backend
// ------------------------------------------------------------------------
// This header replaces rmacxx-mpi-rma.hpp. It is included from rmacxx.hpp
// *inside* namespace rmacxx.
//
// Design
// ------
//  * Window memory lives on the NVSHMEM symmetric heap (GPU memory). Since
//    NVSHMEM allocations are symmetric, every PE allocates the maximum
//    per-PE extent; irregular (per-PE) shapes are still honoured for
//    addressing.
//  * Every data movement is described by a PatchDesc: an N-d box inside a
//    target PE's local array and the matching box in an origin buffer. The
//    same descriptor is executed either
//      - by a kernel launched from the host API (operator<< / operator>>),
//      - or directly by user kernels through DeviceWindow (device-initiated
//        communication, see rmacxx-device.hpp).
//  * PUT/GET move contiguous rows with nvshmem[x]_{put,get}mem_nbi[_warp|_block].
//    Accumulate, get-accumulate, fetch-and-op, compare-and-swap and the
//    ATOMIC_PUT_GET flavours are executed element-wise with NVSHMEM AMOs.
//    NVSHMEM only offers native add for integer types, so other operators
//    (and floating point) go through a compare-and-swap loop on the
//    element's bit pattern; 1- and 2-byte types use a CAS on the enclosing
//    aligned 32-bit word.
//  * Host-API origin buffers (which may be pageable host memory) are staged
//    through a registered device buffer. Gets are copied back to the user's
//    buffer when the operation completes (flush, or immediately for
//    LOCAL_FLUSH/REMOTE_FLUSH windows), mirroring MPI RMA's "data is valid
//    after flush" semantics.
//  * NVSHMEM has no per-target completion, so flush(target),
//    flush_local(target) and flush_all all map to a quiet.
// ------------------------------------------------------------------------

#ifndef RMACXX_MAX_DIMS
#define RMACXX_MAX_DIMS 10
#endif

// threads per block used for host-launched transfer kernels
#ifndef RMACXX_KERNEL_THREADS
#define RMACXX_KERNEL_THREADS 256
#endif

// upper bound on blocks per host-launched transfer kernel
#ifndef RMACXX_MAX_BLOCKS
#define RMACXX_MAX_BLOCKS 1024
#endif

// initial size of the per-window registered staging buffer (bytes);
// it grows on demand
#ifndef RMACXX_STAGING_BYTES
#define RMACXX_STAGING_BYTES ( size_t( 8 ) << 20 )
#endif

// Define RMACXX_DEFER_HOST_GETS to deliver the results of host-API gets
// (and get-accumulate / fetch-and-op / compare-and-swap) only at the next
// flush (or immediately for LOCAL_FLUSH/REMOTE_FLUSH windows), exactly as
// MPI RMA allows. By default they complete before operator>> returns.

// row sizes (bytes) above which host-launched kernels switch from
// thread-per-row to warp-per-row and block-per-row transfers
#ifndef RMACXX_WARP_ROW_BYTES
#define RMACXX_WARP_ROW_BYTES 512
#endif
#ifndef RMACXX_BLOCK_ROW_BYTES
#define RMACXX_BLOCK_ROW_BYTES 16384
#endif

// kernel launch indirection (lets the CPU emulator in tests/emulator run
// the very same kernels)
#ifndef RMACXX_LAUNCH
#define RMACXX_LAUNCH( kern, grid, block, stream, ... ) \
    kern<<<( grid ), ( block ), 0, ( stream )>>>( __VA_ARGS__ )
#endif

// storage class for process-wide singletons (the emulator runs one PE per
// thread and sets this to thread_local)
#ifndef RMACXX_TLS
#define RMACXX_TLS
#endif

// Cooperation scope for device-side bulk operations: the calling thread
// alone, its (full) warp, or its whole thread block. For WARP/BLOCK every
// thread of the group must make the same call with the same arguments.
enum class Scope : int { THREAD = 0, WARP = 1, BLOCK = 2 };

namespace detail
{

// ------------------------------------------------------------------------
// error handling and runtime
// ------------------------------------------------------------------------

struct Runtime
{
    bool initialized = false;
    bool owns_nvshmem = false;
    bool owns_mpi = false;

    static Runtime& get()
    {
        static RMACXX_TLS Runtime r;
        return r;
    }
};

[[noreturn]] inline void fatal( const char* msg, const char* file, int line )
{
    std::fprintf( stderr, "[rmacxx] fatal: %s (%s:%d)\n", msg, file, line );
    std::fflush( stderr );

    if ( Runtime::get().initialized )
        nvshmem_global_exit( 1 );

    std::abort();
}

inline void cuda_check( cudaError_t e, const char* what, const char* file, int line )
{
    if ( e != cudaSuccess )
    {
        std::string msg = std::string( what ) + ": " + cudaGetErrorString( e );
        fatal( msg.c_str(), file, line );
    }
}

#define RMACXX_FATAL( msg ) ::rmacxx::detail::fatal( ( msg ), __FILE__, __LINE__ )
#define RMACXX_CUDA_CHECK( x ) ::rmacxx::detail::cuda_check( ( x ), #x, __FILE__, __LINE__ )

} // namespace detail

// Initialize RMACXX: MPI (if built with the MPI bootstrap and not already
// initialized), GPU selection (node-local rank modulo #GPUs) and NVSHMEM.
// Windows call this lazily, but calling it explicitly (instead of MPI_Init)
// is recommended so that rmacxx::finalize() can tear everything down in
// the right order.
inline void init( int* argc = nullptr, char*** argv = nullptr )
{
    detail::Runtime& rt = detail::Runtime::get();

    if ( rt.initialized )
        return;

#if RMACXX_USE_MPI_BOOTSTRAP
    int mpi_inited = 0;
    MPI_Initialized( &mpi_inited );

    if ( !mpi_inited )
    {
        int provided = 0;
        MPI_Init_thread( argc, argv, MPI_THREAD_MULTIPLE, &provided );
        rt.owns_mpi = true;
    }
#else
    (void)argc;
    (void)argv;
#endif

    if ( nvshmemx_init_status() == NVSHMEM_STATUS_NOT_INITIALIZED )
    {
        int ndev = 0;
        RMACXX_CUDA_CHECK( cudaGetDeviceCount( &ndev ) );

        if ( ndev <= 0 )
            RMACXX_FATAL( "no CUDA device found" );

#if RMACXX_USE_MPI_BOOTSTRAP
        // pick a GPU from the node-local MPI rank *before* NVSHMEM init
        MPI_Comm local;
        int lrank = 0;
        MPI_Comm_split_type( MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0,
                             MPI_INFO_NULL, &local );
        MPI_Comm_rank( local, &lrank );
        MPI_Comm_free( &local );
        RMACXX_CUDA_CHECK( cudaSetDevice( lrank % ndev ) );

        // NVSHMEM keeps using the communicator until nvshmem_finalize
        static MPI_Comm world;
        world = MPI_COMM_WORLD;
#ifdef NVSHMEMX_INIT_ATTR_INITIALIZER
        nvshmemx_init_attr_t attr = NVSHMEMX_INIT_ATTR_INITIALIZER;
#else
        nvshmemx_init_attr_t attr;
#endif
        attr.mpi_comm = &world;

        if ( nvshmemx_init_attr( NVSHMEMX_INIT_WITH_MPI_COMM, &attr ) != 0 )
            RMACXX_FATAL( "nvshmemx_init_attr failed" );
#else
        nvshmem_init();
        RMACXX_CUDA_CHECK( cudaSetDevice( nvshmem_team_my_pe( NVSHMEMX_TEAM_NODE ) % ndev ) );
#endif
        rt.owns_nvshmem = true;
    }

    rt.initialized = true;
}

// Finalize NVSHMEM (and MPI, if rmacxx::init initialized it). Call this
// instead of MPI_Finalize; all windows must have been freed (wfree).
inline void finalize()
{
    detail::Runtime& rt = detail::Runtime::get();

    if ( !rt.initialized )
        return;

    RMACXX_CUDA_CHECK( cudaDeviceSynchronize() );

    if ( rt.owns_nvshmem )
        nvshmem_finalize();

#if RMACXX_USE_MPI_BOOTSTRAP
    if ( rt.owns_mpi )
    {
        int fin = 0;
        MPI_Finalized( &fin );

        if ( !fin )
            MPI_Finalize();
    }
#endif
    rt = detail::Runtime();
}

inline int my_pe() { return nvshmem_my_pe(); }
inline int n_pes() { return nvshmem_n_pes(); }

namespace detail
{

inline void ensure_init()
{
    if ( !Runtime::get().initialized )
        init( nullptr, nullptr );
}

// ------------------------------------------------------------------------
// operators and bit helpers (host + device)
// ------------------------------------------------------------------------

template <typename To, typename From>
__host__ __device__ inline To bitcast_( From const& f )
{
    static_assert( sizeof( To ) == sizeof( From ), "bitcast_: size mismatch" );
    To t;
    memcpy( &t, &f, sizeof( To ) );
    return t;
}

// returns the value to store, given the current target value `cur`
// and the origin operand `val`
template <typename T>
__host__ __device__ inline T apply_op( OpKind op, T cur, T val )
{
    if constexpr ( !std::is_arithmetic<T>::value )
    {
        // non-arithmetic element types only support REPLACE / NO_OP
        return ( op == OpKind::NO_OP ) ? cur : val;
    }
    else
    {
    switch ( op )
    {
    case OpKind::SUM:     return static_cast<T>( cur + val );
    case OpKind::PROD:    return static_cast<T>( cur * val );
    case OpKind::MAX:     return ( cur < val ) ? val : cur;
    case OpKind::MIN:     return ( val < cur ) ? val : cur;
    case OpKind::REPLACE: return val;
    case OpKind::NO_OP:   return cur;
    case OpKind::NONE:    return val;
    default: break;
    }

    if constexpr ( std::is_integral<T>::value )
    {
        switch ( op )
        {
        case OpKind::LAND: return static_cast<T>( ( cur != 0 ) && ( val != 0 ) );
        case OpKind::LOR:  return static_cast<T>( ( cur != 0 ) || ( val != 0 ) );
        case OpKind::LXOR: return static_cast<T>( ( cur != 0 ) != ( val != 0 ) );
        case OpKind::BAND: return static_cast<T>( cur & val );
        case OpKind::BOR:  return static_cast<T>( cur | val );
        case OpKind::BXOR: return static_cast<T>( cur ^ val );
        default: break;
        }
    }

    return cur;
    }
}

template <typename T>
using sub_word_t = typename std::conditional<sizeof( T ) == 1, uint8_t, uint16_t>::type;

template <typename T>
using amo_word_t = typename std::conditional<sizeof( T ) == 8, long long, int>::type;

// ------------------------------------------------------------------------
// atomic layer (device)
// ------------------------------------------------------------------------
// NVSHMEM AMOs used: {int,longlong}_atomic_{fetch,set,swap,compare_swap,
// fetch_add,add}. These exist for all transports (host-side AMOs are only
// supported over NVLink, which is why host-API accumulates launch kernels).

__device__ inline int       amo_w_fetch( int* p, int pe ) { return nvshmem_int_atomic_fetch( p, pe ); }
__device__ inline long long amo_w_fetch( long long* p, int pe ) { return nvshmem_longlong_atomic_fetch( p, pe ); }
__device__ inline void      amo_w_set( int* p, int v, int pe ) { nvshmem_int_atomic_set( p, v, pe ); }
__device__ inline void      amo_w_set( long long* p, long long v, int pe ) { nvshmem_longlong_atomic_set( p, v, pe ); }
__device__ inline int       amo_w_swap( int* p, int v, int pe ) { return nvshmem_int_atomic_swap( p, v, pe ); }
__device__ inline long long amo_w_swap( long long* p, long long v, int pe ) { return nvshmem_longlong_atomic_swap( p, v, pe ); }
__device__ inline int       amo_w_cas( int* p, int c, int v, int pe ) { return nvshmem_int_atomic_compare_swap( p, c, v, pe ); }
__device__ inline long long amo_w_cas( long long* p, long long c, long long v, int pe ) { return nvshmem_longlong_atomic_compare_swap( p, c, v, pe ); }
__device__ inline int       amo_w_fetch_add( int* p, int v, int pe ) { return nvshmem_int_atomic_fetch_add( p, v, pe ); }
__device__ inline long long amo_w_fetch_add( long long* p, long long v, int pe ) { return nvshmem_longlong_atomic_fetch_add( p, v, pe ); }
__device__ inline void      amo_w_add( int* p, int v, int pe ) { nvshmem_int_atomic_add( p, v, pe ); }
__device__ inline void      amo_w_add( long long* p, long long v, int pe ) { nvshmem_longlong_atomic_add( p, v, pe ); }

__device__ inline void amo_unsupported_()
{
#if defined( __CUDA_ARCH__ )
    __trap();
#else
    std::abort();
#endif
}

// 1-/2-byte types: operate on the enclosing aligned 32-bit word. Window
// allocations are padded to 8 bytes, so that word is always symmetric.
template <typename T>
struct SubWord
{
    int* word;
    unsigned shift;
    unsigned mask;

    __device__ explicit SubWord( T* p )
    {
        uintptr_t a = reinterpret_cast<uintptr_t>( p );
        word  = reinterpret_cast<int*>( a & ~uintptr_t( 3 ) );
        shift = static_cast<unsigned>( ( a & 3 ) * 8 );
        mask  = ( sizeof( T ) == 1 ? 0xFFu : 0xFFFFu ) << shift;
    }
    __device__ T extract( int w ) const
    {
        sub_word_t<T> b = static_cast<sub_word_t<T>>( ( static_cast<unsigned>( w ) & mask ) >> shift );
        return bitcast_<T>( b );
    }
    __device__ int insert( int w, T v ) const
    {
        unsigned b = static_cast<unsigned>( bitcast_<sub_word_t<T>>( v ) );
        return static_cast<int>( ( static_cast<unsigned>( w ) & ~mask ) | ( ( b << shift ) & mask ) );
    }
};

// atomically: old = *dest; *dest = apply_op(op, old, val); return old
template <typename T>
__device__ T amo_fetch_op( T* dest, T val, OpKind op, int pe )
{
    if constexpr ( sizeof( T ) == 4 || sizeof( T ) == 8 )
    {
        using W = amo_word_t<T>;
        W* w = reinterpret_cast<W*>( dest );

        if constexpr ( std::is_integral<T>::value )
        {
            if ( op == OpKind::SUM )
                return bitcast_<T>( amo_w_fetch_add( w, bitcast_<W>( val ), pe ) );
        }

        if ( op == OpKind::REPLACE || op == OpKind::NONE )
            return bitcast_<T>( amo_w_swap( w, bitcast_<W>( val ), pe ) );

        if ( op == OpKind::NO_OP )
            return bitcast_<T>( amo_w_fetch( w, pe ) );

        W old = amo_w_fetch( w, pe );

        while ( true )
        {
            T cur = bitcast_<T>( old );
            W nw = bitcast_<W>( apply_op( op, cur, val ) );

            if ( nw == old )
                return cur;

            W prev = amo_w_cas( w, old, nw, pe );

            if ( prev == old )
                return cur;

            old = prev;
        }
    }
    else if constexpr ( sizeof( T ) == 1 || sizeof( T ) == 2 )
    {
        SubWord<T> sw( dest );
        int old = amo_w_fetch( sw.word, pe );

        while ( true )
        {
            T cur = sw.extract( old );
            int nw = sw.insert( old, apply_op( op, cur, val ) );

            if ( nw == old )
                return cur;

            int prev = amo_w_cas( sw.word, old, nw, pe );

            if ( prev == old )
                return cur;

            old = prev;
        }
    }
    else
    {
        amo_unsupported_();
        return T();
    }
}

// non-fetching accumulate
template <typename T>
__device__ void amo_op( T* dest, T val, OpKind op, int pe )
{
    if constexpr ( ( sizeof( T ) == 4 || sizeof( T ) == 8 ) && std::is_integral<T>::value )
    {
        if ( op == OpKind::SUM )
        {
            using W = amo_word_t<T>;
            amo_w_add( reinterpret_cast<W*>( dest ), bitcast_<W>( val ), pe );
            return;
        }
    }

    if constexpr ( sizeof( T ) == 4 || sizeof( T ) == 8 )
    {
        if ( op == OpKind::REPLACE || op == OpKind::NONE )
        {
            using W = amo_word_t<T>;
            amo_w_set( reinterpret_cast<W*>( dest ), bitcast_<W>( val ), pe );
            return;
        }
    }

    (void)amo_fetch_op( dest, val, op, pe );
}

template <typename T>
__device__ T amo_fetch( T* src, int pe )
{
    if constexpr ( sizeof( T ) == 4 || sizeof( T ) == 8 )
    {
        using W = amo_word_t<T>;
        return bitcast_<T>( amo_w_fetch( reinterpret_cast<W*>( src ), pe ) );
    }
    else if constexpr ( sizeof( T ) == 1 || sizeof( T ) == 2 )
    {
        SubWord<T> sw( src );
        return sw.extract( amo_w_fetch( sw.word, pe ) );
    }
    else
    {
        amo_unsupported_();
        return T();
    }
}

template <typename T>
__device__ void amo_set( T* dest, T val, int pe )
{
    amo_op( dest, val, OpKind::REPLACE, pe );
}

// atomically: old = *dest; if (old == cmp) *dest = val; return old
// (comparison is bitwise, like MPI_Compare_and_swap on basic types)
template <typename T>
__device__ T amo_cas( T* dest, T cmp, T val, int pe )
{
    if constexpr ( sizeof( T ) == 4 || sizeof( T ) == 8 )
    {
        using W = amo_word_t<T>;
        return bitcast_<T>( amo_w_cas( reinterpret_cast<W*>( dest ),
                                       bitcast_<W>( cmp ), bitcast_<W>( val ), pe ) );
    }
    else if constexpr ( sizeof( T ) == 1 || sizeof( T ) == 2 )
    {
        SubWord<T> sw( dest );
        int old = amo_w_fetch( sw.word, pe );
        const sub_word_t<T> cbits = bitcast_<sub_word_t<T>>( cmp );

        while ( true )
        {
            T cur = sw.extract( old );

            if ( bitcast_<sub_word_t<T>>( cur ) != cbits )
                return cur;

            int prev = amo_w_cas( sw.word, old, sw.insert( old, val ), pe );

            if ( prev == old )
                return cur;

            old = prev;
        }
    }
    else
    {
        amo_unsupported_();
        return T();
    }
}

// scalar (non-atomic) put/get by value -- no local buffer involved
template <typename T>
__device__ inline void scalar_p( T* dest, T v, int pe )
{
    if constexpr ( sizeof( T ) == 1 )
        nvshmem_uint8_p( reinterpret_cast<uint8_t*>( dest ), bitcast_<uint8_t>( v ), pe );
    else if constexpr ( sizeof( T ) == 2 )
        nvshmem_uint16_p( reinterpret_cast<uint16_t*>( dest ), bitcast_<uint16_t>( v ), pe );
    else if constexpr ( sizeof( T ) == 4 )
        nvshmem_uint32_p( reinterpret_cast<uint32_t*>( dest ), bitcast_<uint32_t>( v ), pe );
    else if constexpr ( sizeof( T ) == 8 )
        nvshmem_uint64_p( reinterpret_cast<uint64_t*>( dest ), bitcast_<uint64_t>( v ), pe );
    else
        nvshmem_putmem( dest, &v, sizeof( T ), pe );
}

template <typename T>
__device__ inline T scalar_g( T* src, int pe )
{
    if constexpr ( sizeof( T ) == 1 )
        return bitcast_<T>( nvshmem_uint8_g( reinterpret_cast<uint8_t*>( src ), pe ) );
    else if constexpr ( sizeof( T ) == 2 )
        return bitcast_<T>( nvshmem_uint16_g( reinterpret_cast<uint16_t*>( src ), pe ) );
    else if constexpr ( sizeof( T ) == 4 )
        return bitcast_<T>( nvshmem_uint32_g( reinterpret_cast<uint32_t*>( src ), pe ) );
    else if constexpr ( sizeof( T ) == 8 )
        return bitcast_<T>( nvshmem_uint64_g( reinterpret_cast<uint64_t*>( src ), pe ) );
    else
    {
        T v;
        nvshmem_getmem( &v, src, sizeof( T ), pe );
        return v;
    }
}

// ------------------------------------------------------------------------
// patch descriptors (host + device)
// ------------------------------------------------------------------------

enum class Xfer : int
{
    PUT = 0,  // origin -> target, rows with putmem
    GET,      // target -> origin, rows with getmem
    APUT,     // atomic put (per element atomic set), ATOMIC_PUT_GET windows
    AGET,     // atomic get (per element atomic fetch), ATOMIC_PUT_GET windows
    ACC,      // per element atomic op (MPI_Accumulate)
    GACC,     // per element fetch-and-op, origin <- old (MPI_Get_accumulate)
    CAS       // per element compare-and-swap: origin holds cmp, operand holds
              // new value, origin <- old (MPI_Compare_and_swap)
};

__host__ __device__ inline bool xfer_is_rowwise( Xfer k ) { return k == Xfer::PUT || k == Xfer::GET; }
__host__ __device__ inline bool xfer_reads_back( Xfer k )
{
    return k == Xfer::GET || k == Xfer::AGET || k == Xfer::GACC || k == Xfer::CAS;
}

// An N-d box in a target PE's local (C-ordered) array and the matching box
// in an (also C-ordered) origin array.
struct PatchDesc
{
    int nd;                           // dimensionality (after normalize())
    int pe;                           // target PE
    int64_t rdims[RMACXX_MAX_DIMS];   // extents of the target's local array
    int64_t rstart[RMACXX_MAX_DIMS];  // patch start in target array
    int64_t odims[RMACXX_MAX_DIMS];   // extents of the origin array
    int64_t ostart[RMACXX_MAX_DIMS];  // patch start in origin array
    int64_t sub[RMACXX_MAX_DIMS];     // patch extents

    __host__ __device__ int64_t count() const
    {
        int64_t c = 1;
        for ( int d = 0; d < nd; d++ ) c *= sub[d];
        return c;
    }
    __host__ __device__ int64_t rowlen() const { return sub[nd-1]; }
    __host__ __device__ int64_t nrows() const
    {
        int64_t c = 1;
        for ( int d = 0; d < nd-1; d++ ) c *= sub[d];
        return c;
    }

    // element offsets (target, origin) of the first element of `row`
    __host__ __device__ void row_offsets( int64_t row, int64_t& roff, int64_t& ooff ) const
    {
        int64_t rs = rdims[nd-1], os = odims[nd-1];
        roff = rstart[nd-1];
        ooff = ostart[nd-1];

        for ( int d = nd-2; d >= 0; d-- )
        {
            const int64_t i = row % sub[d];
            row /= sub[d];
            roff += ( rstart[d] + i ) * rs;
            ooff += ( ostart[d] + i ) * os;
            rs *= rdims[d];
            os *= odims[d];
        }
    }

    // merge dimensions that are fully covered on both sides, so that
    // e.g. a whole-plane patch becomes a single contiguous row
    __host__ __device__ void normalize()
    {
        int d = nd - 1;

        while ( d >= 1 )
        {
            if ( sub[d] == rdims[d] && sub[d] == odims[d] )
            {
                rstart[d-1] = rstart[d-1] * rdims[d] + rstart[d];
                ostart[d-1] = ostart[d-1] * odims[d] + ostart[d];
                rdims[d-1] *= rdims[d];
                odims[d-1] *= odims[d];
                sub[d-1]   *= sub[d];

                for ( int k = d; k < nd-1; k++ )
                {
                    rdims[k]  = rdims[k+1];
                    rstart[k] = rstart[k+1];
                    odims[k]  = odims[k+1];
                    ostart[k] = ostart[k+1];
                    sub[k]    = sub[k+1];
                }

                nd--;
            }

            d--;
        }
    }

    // patch fits in both arrays?
    __host__ __device__ bool valid() const
    {
        if ( nd < 1 || nd > RMACXX_MAX_DIMS ) return false;

        for ( int d = 0; d < nd; d++ )
        {
            if ( sub[d] < 1 || rstart[d] < 0 || ostart[d] < 0 ) return false;
            if ( rstart[d] + sub[d] > rdims[d] ) return false;
            if ( ostart[d] + sub[d] > odims[d] ) return false;
        }

        return true;
    }
};

// contiguous run of `count` elements at element offset `disp` of a target
// array holding `nelems` elements; origin is a packed buffer
__host__ __device__ inline PatchDesc make_linear_desc( int pe, int64_t disp, int64_t count, int64_t nelems )
{
    PatchDesc d;
    d.nd = 1;
    d.pe = pe;
    d.rdims[0] = nelems;
    d.rstart[0] = disp;
    d.odims[0] = count;
    d.ostart[0] = 0;
    d.sub[0] = count;
    return d;
}

// box [lo, lo+sub) of a target array with extents `dims`; origin is
// either a packed buffer of extents `sub` (odims == nullptr) or an array of
// extents `odims` in which the patch starts at `ostart`
template <typename I1, typename I2, typename I3>
__host__ __device__ inline PatchDesc make_box_desc( int pe, int nd, I1 const* dims, I2 const* lo, I3 const* sub,
        int64_t const* odims = nullptr, int64_t const* ostart = nullptr )
{
    PatchDesc d;
    d.nd = nd;
    d.pe = pe;

    for ( int k = 0; k < nd; k++ )
    {
        d.rdims[k]  = dims[k];
        d.rstart[k] = lo[k];
        d.sub[k]    = sub[k];
        d.odims[k]  = odims ? odims[k] : d.sub[k];
        d.ostart[k] = ostart ? ostart[k] : 0;
    }

    return d;
}

// Intersect the global patch [lo, hi] (inclusive) with the block owned by
// PE p (rlo/rhi are [npes x nd] arrays of inclusive ranges). On success
// fills `d` and returns true. The origin is an array of extents `odims`
// (packed patch extents hi-lo+1 if nullptr) in which the patch starts at
// `ostart` (0 if nullptr).
template <typename IR, typename IP>
__host__ __device__ inline bool make_global_piece( PatchDesc& d, int p, int nd, IR const* rlo, IR const* rhi,
        IP const* lo, IP const* hi, int64_t const* odims = nullptr, int64_t const* ostart = nullptr )
{
    for ( int k = 0; k < nd; k++ )
    {
        const int64_t plo = rlo[p*nd+k], phi = rhi[p*nd+k];
        const int64_t ilo = ( (int64_t)lo[k] > plo ) ? (int64_t)lo[k] : plo;
        const int64_t ihi = ( (int64_t)hi[k] < phi ) ? (int64_t)hi[k] : phi;

        if ( ilo > ihi )
            return false;

        d.rdims[k]  = phi - plo + 1;
        d.rstart[k] = ilo - plo;
        d.sub[k]    = ihi - ilo + 1;
        d.odims[k]  = odims ? odims[k] : ( (int64_t)hi[k] - (int64_t)lo[k] + 1 );
        d.ostart[k] = ( ostart ? ostart[k] : 0 ) + ( ilo - (int64_t)lo[k] );
    }

    d.nd = nd;
    d.pe = p;
    return true;
}

// ------------------------------------------------------------------------
// device execution of a patch by a cooperating group
// ------------------------------------------------------------------------

template <Scope S>
__device__ inline int group_rank()
{
    if constexpr ( S == Scope::THREAD )
        return 0;
    else
    {
        const int t = threadIdx.x + blockDim.x * ( threadIdx.y + blockDim.y * threadIdx.z );
        if constexpr ( S == Scope::WARP )
            return t % warpSize;
        else
            return t;
    }
}

template <Scope S>
__device__ inline int group_size()
{
    if constexpr ( S == Scope::THREAD )
        return 1;
    else if constexpr ( S == Scope::WARP )
        return warpSize;
    else
        return blockDim.x * blockDim.y * blockDim.z;
}

template <Scope S>
__device__ inline void group_sync()
{
    if constexpr ( S == Scope::WARP )
        __syncwarp();
    else if constexpr ( S == Scope::BLOCK )
        __syncthreads();
}

template <Scope S>
__device__ inline void put_nbi_g( void* dst, const void* src, size_t bytes, int pe )
{
    if constexpr ( S == Scope::THREAD )
        nvshmem_putmem_nbi( dst, src, bytes, pe );
    else if constexpr ( S == Scope::WARP )
        nvshmemx_putmem_nbi_warp( dst, src, bytes, pe );
    else
        nvshmemx_putmem_nbi_block( dst, src, bytes, pe );
}

template <Scope S>
__device__ inline void get_nbi_g( void* dst, const void* src, size_t bytes, int pe )
{
    if constexpr ( S == Scope::THREAD )
        nvshmem_getmem_nbi( dst, src, bytes, pe );
    else if constexpr ( S == Scope::WARP )
        nvshmemx_getmem_nbi_warp( dst, src, bytes, pe );
    else
        nvshmemx_getmem_nbi_block( dst, src, bytes, pe );
}

// one element of an element-wise transfer
template <typename T>
__device__ inline void exec_elem( Xfer k, OpKind op, T* r, T* o, const T* operand, int64_t ooff, int pe )
{
    switch ( k )
    {
    case Xfer::PUT:
    case Xfer::APUT:
        amo_set( r, *o, pe );
        break;
    case Xfer::GET:
    case Xfer::AGET:
        *o = amo_fetch( r, pe );
        break;
    case Xfer::ACC:
        amo_op( r, *o, op, pe );
        break;
    case Xfer::GACC:
        *o = amo_fetch_op( r, operand[ooff], op, pe );
        break;
    case Xfer::CAS:
        *o = amo_cas( r, *o, operand[ooff], pe );
        break;
    }
}

// Execute patch `d` against the symmetric array `rbase`, with origin
// buffer `obuf` (and `operand` for GACC/CAS; same layout as obuf).
// All threads of scope S must call this with identical arguments.
// Non-blocking: completion requires a quiet.
template <typename T, Scope S>
__device__ void exec_patch( PatchDesc const& d, Xfer k, OpKind op, T* rbase, T* obuf, const T* operand )
{
    const int64_t rl = d.rowlen();

    if ( xfer_is_rowwise( k ) )
    {
        const int64_t nr = d.nrows();

        for ( int64_t r = 0; r < nr; r++ )
        {
            int64_t ro, oo;
            d.row_offsets( r, ro, oo );

            if ( k == Xfer::PUT )
                put_nbi_g<S>( rbase + ro, obuf + oo, rl * sizeof( T ), d.pe );
            else
                get_nbi_g<S>( obuf + oo, rbase + ro, rl * sizeof( T ), d.pe );
        }
    }
    else
    {
        const int64_t n = d.count();

        for ( int64_t e = group_rank<S>(); e < n; e += group_size<S>() )
        {
            int64_t ro, oo;
            d.row_offsets( e / rl, ro, oo );
            ro += e % rl;
            oo += e % rl;
            exec_elem( k, op, rbase + ro, obuf + oo, operand, oo, d.pe );
        }
    }
}

// row distribution modes of the host-launched kernel
enum : int { ROWS_PER_THREAD = 0, ROWS_PER_WARP = 1, ROWS_PER_BLOCK = 2 };

// grid-wide execution of one patch (launched by the host API)
template <typename T>
__global__ void patch_kernel( PatchDesc d, Xfer k, OpKind op, T* rbase, T* obuf, const T* operand, int mode )
{
    const int64_t rl = d.rowlen();

    if ( xfer_is_rowwise( k ) )
    {
        const int64_t nr = d.nrows();
        int64_t first, stride;

        if ( mode == ROWS_PER_BLOCK )
        {
            first = blockIdx.x;
            stride = gridDim.x;
        }
        else if ( mode == ROWS_PER_WARP )
        {
            const int64_t wpb = ( blockDim.x + warpSize - 1 ) / warpSize;
            first = (int64_t)blockIdx.x * wpb + threadIdx.x / warpSize;
            stride = (int64_t)gridDim.x * wpb;
        }
        else
        {
            first = (int64_t)blockIdx.x * blockDim.x + threadIdx.x;
            stride = (int64_t)gridDim.x * blockDim.x;
        }

        for ( int64_t r = first; r < nr; r += stride )
        {
            int64_t ro, oo;
            d.row_offsets( r, ro, oo );
            const size_t bytes = rl * sizeof( T );

            if ( mode == ROWS_PER_BLOCK )
            {
                if ( k == Xfer::PUT ) put_nbi_g<Scope::BLOCK>( rbase + ro, obuf + oo, bytes, d.pe );
                else                  get_nbi_g<Scope::BLOCK>( obuf + oo, rbase + ro, bytes, d.pe );
            }
            else if ( mode == ROWS_PER_WARP )
            {
                if ( k == Xfer::PUT ) put_nbi_g<Scope::WARP>( rbase + ro, obuf + oo, bytes, d.pe );
                else                  get_nbi_g<Scope::WARP>( obuf + oo, rbase + ro, bytes, d.pe );
            }
            else
            {
                if ( k == Xfer::PUT ) put_nbi_g<Scope::THREAD>( rbase + ro, obuf + oo, bytes, d.pe );
                else                  get_nbi_g<Scope::THREAD>( obuf + oo, rbase + ro, bytes, d.pe );
            }
        }
    }
    else
    {
        const int64_t n = d.count();
        const int64_t stride = (int64_t)gridDim.x * blockDim.x;

        for ( int64_t e = (int64_t)blockIdx.x * blockDim.x + threadIdx.x; e < n; e += stride )
        {
            int64_t ro, oo;
            d.row_offsets( e / rl, ro, oo );
            ro += e % rl;
            oo += e % rl;
            exec_elem( k, op, rbase + ro, obuf + oo, operand, oo, d.pe );
        }
    }
}

template <typename T>
__global__ void scale_kernel( T* p, int64_t n, T s )
{
    for ( int64_t i = (int64_t)blockIdx.x * blockDim.x + threadIdx.x; i < n; i += (int64_t)gridDim.x * blockDim.x )
        p[i] = static_cast<T>( p[i] * s );
}

// ------------------------------------------------------------------------
// host side: origin layouts and the per-window transfer engine
// ------------------------------------------------------------------------

// Layout of a user origin buffer: an array of extents `odims`, in which the
// patch of extents `ext` starts at `ostart`.
struct HostLayout
{
    int nd = 1;
    int64_t ext[RMACXX_MAX_DIMS]    = {};
    int64_t odims[RMACXX_MAX_DIMS]  = {};
    int64_t ostart[RMACXX_MAX_DIMS] = {};

    template <typename I>
    static HostLayout packed( int nd, I const* ext )
    {
        HostLayout l;
        l.nd = nd;

        for ( int k = 0; k < nd; k++ )
        {
            l.ext[k] = ext[k];
            l.odims[k] = ext[k];
            l.ostart[k] = 0;
        }

        return l;
    }

    static HostLayout packed1( int64_t n )
    {
        return packed( 1, &n );
    }

    int64_t count() const
    {
        int64_t c = 1;
        for ( int k = 0; k < nd; k++ ) c *= ext[k];
        return c;
    }

    // is the patch one contiguous run inside the origin array? if so,
    // returns its element offset in `off`
    bool contiguous( int64_t& off ) const
    {
        // leading unit extents do not matter; every dimension after the
        // first non-unit one must be fully covered
        int first = 0;
        while ( first < nd-1 && ext[first] == 1 ) first++;
        for ( int k = first+1; k < nd; k++ )
            if ( ext[k] != odims[k] ) return false;

        off = 0;
        for ( int k = 0; k < nd; k++ )
            off = off * odims[k] + ostart[k];

        return true;
    }

    // packed <-> origin array row mapping, reusing PatchDesc arithmetic
    PatchDesc as_desc() const
    {
        PatchDesc d;
        d.nd = nd;
        d.pe = 0;

        for ( int k = 0; k < nd; k++ )
        {
            d.rdims[k] = odims[k];
            d.rstart[k] = ostart[k];
            d.odims[k] = ext[k];
            d.ostart[k] = 0;
            d.sub[k] = ext[k];
        }

        return d;
    }

    bool valid() const
    {
        PatchDesc d = as_desc();
        return d.valid();
    }
};

template <typename T>
class Engine
{
public:
    Engine() = default;
    Engine( const Engine& ) = delete;
    Engine& operator=( const Engine& ) = delete;

    ~Engine()
    {
        // non-collective resources only; symmetric memory is released by
        // destroy() (collective), called from wfree()
        release_local_();
    }

    // allgather of n trivially copyable items per PE (collective)
    template <typename U>
    static std::vector<U> allgather( U const* mine, size_t n )
    {
        ensure_init();
        const int me = nvshmem_my_pe(), np = nvshmem_n_pes();
        const size_t bytes = n * sizeof( U );
        const size_t slot = ( bytes + 15 ) & ~size_t( 15 );
        char* sym = static_cast<char*>( nvshmem_malloc( slot * ( np + 1 ) ) );

        if ( sym == nullptr )
            RMACXX_FATAL( "nvshmem_malloc failed (allgather scratch)" );

        RMACXX_CUDA_CHECK( cudaMemcpy( sym + slot * np, mine, bytes, cudaMemcpyDefault ) );
        RMACXX_CUDA_CHECK( cudaDeviceSynchronize() );
        nvshmem_barrier_all();

        for ( int p = 0; p < np; p++ )
            nvshmem_putmem( sym + slot * me, sym + slot * np, bytes, p );

        nvshmem_barrier_all();
        std::vector<U> out( n * np );

        for ( int p = 0; p < np; p++ )
            RMACXX_CUDA_CHECK( cudaMemcpy( out.data() + n * p, sym + slot * p, bytes, cudaMemcpyDefault ) );

        nvshmem_free( sym );
        return out;
    }

    // collective: allocate the symmetric window buffer
    void create( int64_t local_elems )
    {
        ensure_init();
        me_ = nvshmem_my_pe();
        npes_ = nvshmem_n_pes();
        local_elems_ = local_elems;

        long long mine = static_cast<long long>( local_elems );
        std::vector<long long> all = allgather( &mine, 1 );
        long long mx = 1;

        for ( long long v : all ) mx = ( v > mx ) ? v : mx;

        sym_elems_ = mx;
        const size_t bytes = ( static_cast<size_t>( mx ) * sizeof( T ) + 7 ) & ~size_t( 7 );
        base_ = static_cast<T*>( nvshmem_malloc( bytes ) );

        if ( base_ == nullptr )
            RMACXX_FATAL( "nvshmem_malloc failed (window buffer); increase NVSHMEM_SYMMETRIC_SIZE" );

        RMACXX_CUDA_CHECK( cudaMemset( base_, 0, bytes ) );
        RMACXX_CUDA_CHECK( cudaStreamCreateWithFlags( &stream_, cudaStreamNonBlocking ) );
        RMACXX_CUDA_CHECK( cudaDeviceSynchronize() );
        nvshmem_barrier_all();
    }

    // collective: release everything
    void destroy()
    {
        if ( base_ == nullptr )
            return;

        complete();
        release_local_();
        nvshmem_free( base_ );   // collective, implies a barrier
        base_ = nullptr;
    }

    T* base() const { return base_; }
    int me() const { return me_; }
    int npes() const { return npes_; }
    int64_t local_elems() const { return local_elems_; }
    int64_t sym_elems() const { return sym_elems_; }
    cudaStream_t stream() const { return stream_; }
    bool created() const { return base_ != nullptr; }

    // device buffer used by expressions for deferred gets
    T* alloc_pbuf( size_t n )
    {
        if ( pbuf_ == nullptr )
        {
            RMACXX_CUDA_CHECK( cudaMalloc( &pbuf_, n * sizeof( T ) ) );
            register_( pbuf_, n * sizeof( T ) );
        }

        return pbuf_;
    }

    // Issue a transfer between the user's origin buffer(s) and the window.
    //  pieces : descriptors whose origin side refers to the *packed* patch
    //           (extents lay.ext), see make_box_desc / make_global_piece
    //  lay    : where the patch lives in the user's buffers
    //  in     : origin data (PUT/APUT/ACC), compare values (CAS)
    //  out    : result buffer (GET/AGET/GACC/CAS); may alias `in` for CAS
    //  operand: GACC operand / CAS new values (same layout as in/out)
    //  oplay  : layout of `operand` if it differs from `lay`
    void issue( std::vector<PatchDesc>& pieces, HostLayout const& lay, Xfer k, OpKind op,
                const T* in, T* out, const T* operand, bool complete_now,
                HostLayout const* oplay = nullptr )
    {
        std::lock_guard<std::recursive_mutex> g( mtx_ );
        if ( !lay.valid() )
            RMACXX_FATAL( "origin buffer layout does not contain the requested patch" );

        const int64_t n = lay.count();
        const bool two = ( k == Xfer::GACC || k == Xfer::CAS );
        T* s = stage_alloc_( static_cast<size_t>( n ) * ( two ? 2 : 1 ) );
        T* s2 = two ? s + n : nullptr;

        switch ( k )
        {
        case Xfer::PUT:
        case Xfer::APUT:
        case Xfer::ACC:
        case Xfer::CAS:
            pack_( in, lay, s );
            break;
        default:
            break;
        }

        if ( two )
        {
            if ( oplay != nullptr && oplay->count() != n )
                RMACXX_FATAL( "operand layout does not match the patch" );

            pack_( operand, oplay ? *oplay : lay, s2 );
        }

        for ( auto& d : pieces )
            launch_( d, k, op, s, s2 );

        if ( xfer_reads_back( k ) )
        {
            copybacks_.push_back( Copyback{ s, out, lay } );
#ifndef RMACXX_DEFER_HOST_GETS
            // By default results are delivered before returning: codes
            // commonly (if not strictly legally) read get results before
            // flushing, and a deferred copy-back into a buffer that has
            // gone out of scope would corrupt memory.
            complete_now = true;
#endif
        }

        if ( complete_now )
            complete();
    }

    // Issue a transfer whose origin is already a device buffer accessible
    // to NVSHMEM (registered or symmetric), laid out as the packed patch.
    void issue_device( std::vector<PatchDesc>& pieces, Xfer k, OpKind op, T* dev_origin, const T* dev_operand )
    {
        std::lock_guard<std::recursive_mutex> g( mtx_ );
        for ( auto& d : pieces )
            launch_( d, k, op, dev_origin, dev_operand );
    }

    // does completing now have any effect for the user's buffers?
    bool has_copybacks() const { return !copybacks_.empty(); }

    // complete all outstanding operations of this window: remote
    // completion of puts/accumulates, and delivery of gets into the user's
    // buffers
    void complete()
    {
        std::lock_guard<std::recursive_mutex> g( mtx_ );

        if ( pending_ )
        {
            nvshmemx_quiet_on_stream( stream_ );
            RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
            pending_ = false;
        }

        for ( auto& cb : copybacks_ )
            unpack_( cb.dev, cb.lay, cb.host );

        copybacks_.clear();
        stage_off_ = 0;
    }

    void complete_local()
    {
        std::lock_guard<std::recursive_mutex> g( mtx_ );

        if ( has_copybacks() )
            complete();
    }

    // stream-ordered barrier across all PEs, after completing everything
    // (including device-initiated operations from user kernels)
    void barrier()
    {
        RMACXX_CUDA_CHECK( cudaDeviceSynchronize() );
        complete();
        nvshmemx_barrier_all_on_stream( stream_ );
        RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
    }

    // synchronous copies ordered with this window's stream
    void h2d( void* dst, const void* src, size_t bytes )
    {
        RMACXX_CUDA_CHECK( cudaMemcpyAsync( dst, src, bytes, cudaMemcpyDefault, stream_ ) );
        RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
    }
    void d2h( void* dst, const void* src, size_t bytes )
    {
        RMACXX_CUDA_CHECK( cudaMemcpyAsync( dst, src, bytes, cudaMemcpyDefault, stream_ ) );
        RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
    }

    void scale_local( T s )
    {
        const int64_t n = local_elems_;
        int grid = static_cast<int>( std::min<int64_t>( RMACXX_MAX_BLOCKS,
                                     std::max<int64_t>( 1, ( n + RMACXX_KERNEL_THREADS - 1 ) / RMACXX_KERNEL_THREADS ) ) );
        RMACXX_LAUNCH( scale_kernel<T>, grid, RMACXX_KERNEL_THREADS, stream_, base_, n, s );
        RMACXX_CUDA_CHECK( cudaGetLastError() );
        RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
    }

private:
    struct Copyback
    {
        T* dev;
        T* host;
        HostLayout lay;
    };

    void register_( void* p, size_t bytes )
    {
        if ( nvshmemx_buffer_register( p, bytes ) != 0 )
            RMACXX_FATAL( "nvshmemx_buffer_register failed" );
    }

    void release_local_()
    {
        // after rmacxx::finalize() NVSHMEM must not be called any more
        // (window objects may be destroyed after finalize)
        if ( !Runtime::get().initialized )
        {
            stage_ = nullptr;
            pbuf_ = nullptr;
            stream_ = nullptr;
            return;
        }

        if ( stage_ != nullptr )
        {
            nvshmemx_buffer_unregister( stage_ );
            cudaFree( stage_ );
            stage_ = nullptr;
            stage_cap_ = 0;
        }

        if ( pbuf_ != nullptr )
        {
            nvshmemx_buffer_unregister( pbuf_ );
            cudaFree( pbuf_ );
            pbuf_ = nullptr;
        }

        if ( stream_ != nullptr )
        {
            cudaStreamDestroy( stream_ );
            stream_ = nullptr;
        }
    }

    // bump allocation from the registered staging buffer; staging is
    // recycled when all outstanding operations complete
    T* stage_alloc_( size_t n )
    {
        const size_t bytes = ( std::max<size_t>( n, 1 ) * sizeof( T ) + 255 ) & ~size_t( 255 );

        if ( stage_off_ + bytes > stage_cap_ )
        {
            complete();

            if ( bytes > stage_cap_ )
            {
                if ( stage_ != nullptr )
                {
                    nvshmemx_buffer_unregister( stage_ );
                    RMACXX_CUDA_CHECK( cudaFree( stage_ ) );
                }

                stage_cap_ = std::max<size_t>( { bytes, 2 * stage_cap_, size_t( RMACXX_STAGING_BYTES ) } );
                RMACXX_CUDA_CHECK( cudaMalloc( &stage_, stage_cap_ ) );
                register_( stage_, stage_cap_ );
            }
        }

        T* p = reinterpret_cast<T*>( stage_ + stage_off_ );
        stage_off_ += bytes;
        return p;
    }

    // is `p` ordinary (pageable, unregistered) host memory? cudaMemcpyAsync
    // from such memory consumes the source before returning
    static bool is_pageable_( const void* p )
    {
        cudaPointerAttributes attr;

        if ( cudaPointerGetAttributes( &attr, p ) != cudaSuccess )
        {
            (void)cudaGetLastError();
            return true;
        }

        return attr.type == cudaMemoryTypeUnregistered;
    }

    // user layout -> packed device buffer (stream ordered before the
    // transfer kernels; the user's buffer is reusable on return)
    void pack_( const T* src, HostLayout const& lay, T* dev )
    {
        int64_t off = 0;
        const int64_t n = lay.count();

        if ( lay.contiguous( off ) )
        {
            RMACXX_CUDA_CHECK( cudaMemcpyAsync( dev, src + off, n * sizeof( T ), cudaMemcpyDefault, stream_ ) );

            if ( !is_pageable_( src ) )
                RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );

            return;
        }

        PatchDesc m = lay.as_desc();
        const int64_t nr = m.nrows(), rl = m.rowlen();
        std::vector<T> tmp( static_cast<size_t>( n ) );

        for ( int64_t r = 0; r < nr; r++ )
        {
            int64_t uo, po;
            m.row_offsets( r, uo, po );
            RMACXX_CUDA_CHECK( cudaMemcpyAsync( tmp.data() + po, src + uo, rl * sizeof( T ), cudaMemcpyDefault, stream_ ) );
        }

        RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
        // pageable source: consumed before cudaMemcpyAsync returns
        RMACXX_CUDA_CHECK( cudaMemcpyAsync( dev, tmp.data(), n * sizeof( T ), cudaMemcpyDefault, stream_ ) );
    }

    // packed device buffer -> user layout
    void unpack_( const T* dev, HostLayout const& lay, T* dst )
    {
        int64_t off = 0;
        const int64_t n = lay.count();

        if ( lay.contiguous( off ) )
        {
            d2h( dst + off, dev, n * sizeof( T ) );
            return;
        }

        std::vector<T> tmp( static_cast<size_t>( n ) );
        d2h( tmp.data(), dev, n * sizeof( T ) );
        PatchDesc m = lay.as_desc();
        const int64_t nr = m.nrows(), rl = m.rowlen();

        for ( int64_t r = 0; r < nr; r++ )
        {
            int64_t uo, po;
            m.row_offsets( r, uo, po );
            RMACXX_CUDA_CHECK( cudaMemcpyAsync( dst + uo, tmp.data() + po, rl * sizeof( T ), cudaMemcpyDefault, stream_ ) );
        }

        RMACXX_CUDA_CHECK( cudaStreamSynchronize( stream_ ) );
    }

    void launch_( PatchDesc d, Xfer k, OpKind op, T* obuf, const T* operand )
    {
        if ( d.pe < 0 || d.pe >= npes_ )
            RMACXX_FATAL( "target PE out of range" );

        if ( !d.valid() )
            RMACXX_FATAL( "patch exceeds the bounds of the target's window" );

        d.normalize();

        const int64_t threads = RMACXX_KERNEL_THREADS;
        int64_t blocks;
        int mode = ROWS_PER_THREAD;

        if ( xfer_is_rowwise( k ) )
        {
            const int64_t nr = d.nrows();
            const int64_t rowbytes = d.rowlen() * static_cast<int64_t>( sizeof( T ) );
            const int64_t wpb = std::max<int64_t>( 1, threads / 32 );

            if ( rowbytes >= RMACXX_BLOCK_ROW_BYTES )
            {
                mode = ROWS_PER_BLOCK;
                blocks = nr;
            }
            else if ( rowbytes >= RMACXX_WARP_ROW_BYTES )
            {
                mode = ROWS_PER_WARP;
                blocks = ( nr + wpb - 1 ) / wpb;
            }
            else
                blocks = ( nr + threads - 1 ) / threads;
        }
        else
            blocks = ( d.count() + threads - 1 ) / threads;

        const int grid = static_cast<int>( std::min<int64_t>( RMACXX_MAX_BLOCKS, std::max<int64_t>( 1, blocks ) ) );
        RMACXX_LAUNCH( patch_kernel<T>, grid, static_cast<int>( threads ), stream_, d, k, op, base_, obuf, operand, mode );
        RMACXX_CUDA_CHECK( cudaGetLastError() );
        pending_ = true;
    }

    T* base_ = nullptr;
    int me_ = -1, npes_ = 0;
    int64_t local_elems_ = 0, sym_elems_ = 0;
    cudaStream_t stream_ = nullptr;

    char* stage_ = nullptr;
    size_t stage_cap_ = 0, stage_off_ = 0;
    T* pbuf_ = nullptr;

    bool pending_ = false;
    std::vector<Copyback> copybacks_;
    // CONCURRENT windows: completion may race with issuing from other threads
    std::recursive_mutex mtx_;
};

// ------------------------------------------------------------------------
// expression support
// ------------------------------------------------------------------------
// Every window access inside an expression (win(...) used as an EExpr /
// BExpr operand) creates one ExprRec. RefEExpr/RefBExpr remember the index
// of their record, so gets, evaluation and puts always refer to the right
// access regardless of the order in which the compiler evaluates operands.
// Gets are issued (non-blocking) into a registered device buffer and
// copied to a host mirror when the first result is needed.

struct ExprRec
{
    std::vector<PatchDesc> pieces;   // origin side refers to packed patch
    HostLayout lay;                  // packed patch layout
    int64_t count = 0;
    int64_t pbuf_off = -1;           // offset into prefetch buffer, or -1
    bool is_dest = false;            // window on the RHS of >> (a put)
    bool put_done = false;
};

template <typename T>
class ExprState
{
public:
    void init( Engine<T>& eng )
    {
        if ( pbuf_dev_ == nullptr )
        {
            pbuf_dev_ = eng.alloc_pbuf( PREALLOC_BUF_SZ );
            host_.resize( PREALLOC_BUF_SZ );
        }
    }

    int push( ExprRec&& r )
    {
        recs_.push_back( std::move( r ) );
        return static_cast<int>( recs_.size() ) - 1;
    }

    ExprRec& rec( int idx )
    {
        if ( idx < 0 || idx >= static_cast<int>( recs_.size() ) )
            RMACXX_FATAL( "expression refers to a window access that was already flushed" );

        return recs_[idx];
    }

    int64_t count( int idx ) { return rec( idx ).count; }

    // post the get of access `idx` into the prefetch buffer (if room)
    void issue_get( Engine<T>& eng, int idx, bool atomic )
    {
        ExprRec& r = rec( idx );

        if ( r.is_dest || r.pbuf_off >= 0 )
            return;

        if ( used_ + r.count <= PREALLOC_BUF_SZ )
        {
            eng.issue_device( r.pieces, atomic ? Xfer::AGET : Xfer::GET, OpKind::NONE, pbuf_dev_ + used_, nullptr );
            r.pbuf_off = used_;
            used_ += r.count;
            dirty_ = true;
        }
    }

    // complete outstanding prefetches and refresh the host mirror
    void sync( Engine<T>& eng )
    {
        if ( dirty_ )
        {
            eng.complete();
            eng.d2h( host_.data(), pbuf_dev_, static_cast<size_t>( used_ ) * sizeof( T ) );
            dirty_ = false;
        }
    }

    int64_t fill_into( Engine<T>& eng, int idx, T* buf, bool atomic )
    {
        ExprRec& r = rec( idx );

        if ( r.pbuf_off >= 0 )
        {
            sync( eng );
            std::memcpy( buf, host_.data() + r.pbuf_off, static_cast<size_t>( r.count ) * sizeof( T ) );
        }
        else // not prefetched (no room): blocking get
            eng.issue( r.pieces, r.lay, atomic ? Xfer::AGET : Xfer::GET, OpKind::NONE, nullptr, buf, nullptr, true );

        return r.count;
    }

    T eval( Engine<T>& eng, int idx, bool atomic )
    {
        T val = T();
        fill_into( eng, idx, &val, atomic );
        return val;
    }

    void mark_dest( int idx ) { rec( idx ).is_dest = true; }

    // put the result of an expression into access `idx` (once)
    void put( Engine<T>& eng, int idx, const T* buf, bool atomic )
    {
        ExprRec& r = rec( idx );

        if ( r.put_done )
            return;

        eng.issue( r.pieces, r.lay, atomic ? Xfer::APUT : Xfer::PUT, OpKind::NONE, buf, nullptr, nullptr, false );
        r.put_done = true;
    }

    void clear( Engine<T>& eng )
    {
        if ( used_ > 0 )
            eng.complete();

        recs_.clear();
        used_ = 0;
        dirty_ = false;
    }

private:
    std::vector<ExprRec> recs_;
    T* pbuf_dev_ = nullptr;
    std::vector<T> host_;
    int64_t used_ = 0;
    bool dirty_ = false;
};

#if RMACXX_USE_MPI_BOOTSTRAP
// Windows always span all PEs (NVSHMEM symmetric allocation is collective
// over NVSHMEM_TEAM_WORLD). Communicator arguments are accepted for source
// compatibility, but must be congruent with MPI_COMM_WORLD.
inline void check_world_comm( MPI_Comm comm )
{
    int res = MPI_UNEQUAL;
    MPI_Comm_compare( comm, MPI_COMM_WORLD, &res );

    if ( res != MPI_IDENT && res != MPI_CONGRUENT )
        RMACXX_FATAL( "RMACXX/NVSHMEM windows must span MPI_COMM_WORLD (sub-communicators are not supported)" );
}
#endif

} // namespace detail
