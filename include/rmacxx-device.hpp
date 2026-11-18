// ------------------------------------------------------------------------
// RMACXX device-side windows (device-initiated communication)
// ------------------------------------------------------------------------
// Included from rmacxx.hpp inside namespace rmacxx.
//
// A DeviceWindow is a small, trivially copyable view of a Window that can
// be passed by value to a kernel. Obtain it on the host with
//     auto dw = win.device_view();
// and use it inside kernels, e.g. (local view)
//     dw.put( target, {0, 0}, {4, 4}, src );                // blocking
//     dw.put_nbi<rmacxx::Scope::BLOCK>( target, lo, hi, src ); // whole block
//     dw.acc( target, {i, j}, 1.0, SUM );                    // atomic
//     dw.quiet();
// or (global view, arithmetic global coordinates; patches may span PEs)
//     dw.get( {2, 4, 1}, {7, 8, 8}, dst );
//
// Semantics
//  * *_nbi bulk calls are non-blocking: data movement is complete (local
//    and remote) after quiet() (or a barrier). The blocking variants
//    (put/get/acc/gacc without _nbi) call quiet() before returning.
//  * Scope::WARP / Scope::BLOCK variants are collective: every thread of
//    the (fully active) warp / block must call them with the same arguments.
//  * Origin/result buffers of bulk calls are packed C-ordered arrays of
//    extents (hi-lo+1) unless an Origin describing a strided buffer is
//    passed. With NVLink/P2P any device memory works; over remote network
//    transports (IB/IBGDA, UCX, libfabric) NVSHMEM requires local buffers
//    to be symmetric (nvshmem_malloc) or registered with
//    nvshmemx_buffer_register (see rmacxx::register_buffer).
//  * Element-wise accessors (put/get with a single coordinate) pass values
//    in registers and have no such requirement.
//  * acc/fetch_op/cas work for 1, 2, 4 and 8 byte arithmetic types; ops
//    other than integer SUM are emulated with a compare-and-swap loop.
// ------------------------------------------------------------------------

// N-d coordinate usable on host and device: Coord c = {i, j, k};
struct Coord
{
    int n;
    int64_t v[RMACXX_MAX_DIMS];

    __host__ __device__ Coord() : n( 0 ), v{} {}

    template <typename... I,
              typename std::enable_if<( sizeof...( I ) >= 1 ) &&
                                      ( sizeof...( I ) <= RMACXX_MAX_DIMS ) &&
                                      std::conjunction<std::is_integral<I>...>::value, int>::type = 0>
    __host__ __device__ Coord( I... is ) : n( static_cast<int>( sizeof...( I ) ) ), v{ static_cast<int64_t>( is )... } {}

    template <typename I>
    __host__ __device__ Coord( I const* p, int nd ) : n( nd ), v{}
    {
        for ( int k = 0; k < nd; k++ ) v[k] = static_cast<int64_t>( p[k] );
    }

    __host__ __device__ int64_t operator[]( int i ) const { return v[i]; }
    __host__ __device__ int64_t& operator[]( int i ) { return v[i]; }
};

// A strided origin buffer: an array `ptr` of extents `dims` (C order) in
// which the patch starts at `starts` (the device analog of
// RMACXX_Subarray_t).
template <typename T>
struct Origin
{
    T* ptr;
    Coord dims;
    Coord starts;

    __host__ __device__ Origin( T* p, Coord const& d, Coord const& s ) : ptr( p ), dims( d ), starts( s ) {}
    __host__ __device__ Origin( const T* p, Coord const& d, Coord const& s ) : ptr( const_cast<T*>( p ) ), dims( d ), starts( s ) {}
};

// Register a local (non-symmetric) device or host buffer so it can be used
// as the origin of device-initiated operations over network transports.
inline void register_buffer( void* p, size_t bytes )
{
    if ( nvshmemx_buffer_register( p, bytes ) != 0 )
        RMACXX_FATAL( "nvshmemx_buffer_register failed" );
}
inline void unregister_buffer( void* p ) { nvshmemx_buffer_unregister( p ); }

template <typename T, WinType wtype_> struct DeviceWindow {};

namespace detail
{
// complete everything the group issued: synchronize so all operations have
// been issued, then a single elected thread calls quiet (NVSHMEM guarantees
// this completes the operations of the whole group), then release the group
template <Scope S>
__device__ inline void group_quiet()
{
    group_sync<S>();

    if ( group_rank<S>() == 0 )
        nvshmem_quiet();

    group_sync<S>();
}
} // namespace detail

// ------------------------------------------------------------------------
// local view: (target PE, local coordinates)
// ------------------------------------------------------------------------
template <typename T>
struct DeviceWindow<T, LOCAL_VIEW>
{
    T* base;                         // symmetric address of the window
    int ndims;
    int64_t dims[RMACXX_MAX_DIMS];   // local array extents (same on all PEs)
    int64_t nelems;
    int me, npes;

    __device__ int rank() const { return me; }
    __device__ int size() const { return npes; }
    // this PE's part of the window
    __device__ T* local() const { return base; }
    // direct load/store pointer to `pe`'s part, or nullptr if `pe` is not
    // reachable by load/store (e.g. over the network)
    __device__ T* ptr( int pe ) const { return static_cast<T*>( nvshmem_ptr( base, pe ) ); }

    __host__ __device__ int64_t offset( Coord const& c ) const
    {
        if ( ndims == 1 || c.n == 1 )
            return c.v[0];

        int64_t off = 0;
        for ( int k = 0; k < ndims; k++ ) off = off * dims[k] + c.v[k];
        return off;
    }

    // ---- element-wise ------------------------------------------------
    __device__ void put( int pe, Coord const& at, T val ) const { detail::scalar_p( base + offset( at ), val, pe ); }
    __device__ T get( int pe, Coord const& at ) const { return detail::scalar_g( base + offset( at ), pe ); }
    __device__ void atomic_put( int pe, Coord const& at, T val ) const { detail::amo_set( base + offset( at ), val, pe ); }
    __device__ T atomic_get( int pe, Coord const& at ) const { return detail::amo_fetch( base + offset( at ), pe ); }
    // non-fetching accumulate (complete after quiet)
    __device__ void acc( int pe, Coord const& at, T val, Op op = SUM ) const { detail::amo_op( base + offset( at ), val, op.op, pe ); }
    __device__ T fetch_op( int pe, Coord const& at, T val, Op op = SUM ) const { return detail::amo_fetch_op( base + offset( at ), val, op.op, pe ); }
    // returns the previous value; stores `val` if previous == `cmp`
    __device__ T cas( int pe, Coord const& at, T cmp, T val ) const { return detail::amo_cas( base + offset( at ), cmp, val, pe ); }

    // ---- bulk: box [lo, hi] (inclusive) of PE `pe` -------------------
    template <Scope S = Scope::THREAD>
    __device__ void put_nbi( int pe, Coord const& lo, Coord const& hi, const T* src ) const { xfer_<S>( detail::Xfer::PUT, OpKind::NONE, pe, lo, hi, const_cast<T*>( src ), nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void get_nbi( int pe, Coord const& lo, Coord const& hi, T* dst ) const { xfer_<S>( detail::Xfer::GET, OpKind::NONE, pe, lo, hi, dst, nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void acc_nbi( int pe, Coord const& lo, Coord const& hi, const T* src, Op op = SUM ) const { xfer_<S>( detail::Xfer::ACC, op.op, pe, lo, hi, const_cast<T*>( src ), nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void atomic_put_nbi( int pe, Coord const& lo, Coord const& hi, const T* src ) const { xfer_<S>( detail::Xfer::APUT, OpKind::NONE, pe, lo, hi, const_cast<T*>( src ), nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void atomic_get_nbi( int pe, Coord const& lo, Coord const& hi, T* dst ) const { xfer_<S>( detail::Xfer::AGET, OpKind::NONE, pe, lo, hi, dst, nullptr, nullptr ); }

    template <Scope S = Scope::THREAD>
    __device__ void put( int pe, Coord const& lo, Coord const& hi, const T* src ) const { put_nbi<S>( pe, lo, hi, src ); detail::group_quiet<S>(); }
    template <Scope S = Scope::THREAD>
    __device__ void get( int pe, Coord const& lo, Coord const& hi, T* dst ) const { get_nbi<S>( pe, lo, hi, dst ); detail::group_quiet<S>(); }
    template <Scope S = Scope::THREAD>
    __device__ void acc( int pe, Coord const& lo, Coord const& hi, const T* src, Op op = SUM ) const { acc_nbi<S>( pe, lo, hi, src, op ); detail::group_quiet<S>(); }
    // get-accumulate: result <- old values, target <- op(old, operand)
    template <Scope S = Scope::THREAD>
    __device__ void gacc( int pe, Coord const& lo, Coord const& hi, const T* operand, T* result, Op op = SUM ) const
    { xfer_<S>( detail::Xfer::GACC, op.op, pe, lo, hi, result, operand, nullptr ); detail::group_quiet<S>(); }

    // strided origin variants
    template <Scope S = Scope::THREAD>
    __device__ void put_nbi( int pe, Coord const& lo, Coord const& hi, Origin<T> const& o ) const { xfer_<S>( detail::Xfer::PUT, OpKind::NONE, pe, lo, hi, o.ptr, nullptr, &o ); }
    template <Scope S = Scope::THREAD>
    __device__ void get_nbi( int pe, Coord const& lo, Coord const& hi, Origin<T> const& o ) const { xfer_<S>( detail::Xfer::GET, OpKind::NONE, pe, lo, hi, o.ptr, nullptr, &o ); }
    template <Scope S = Scope::THREAD>
    __device__ void acc_nbi( int pe, Coord const& lo, Coord const& hi, Origin<T> const& o, Op op = SUM ) const { xfer_<S>( detail::Xfer::ACC, op.op, pe, lo, hi, o.ptr, nullptr, &o ); }

    __device__ void quiet() const { nvshmem_quiet(); }
    __device__ void fence() const { nvshmem_fence(); }

    template <Scope S>
    __device__ void xfer_( detail::Xfer k, OpKind op, int pe, Coord const& lo, Coord const& hi,
                           T* obuf, const T* operand, Origin<T> const* o ) const
    {
        detail::PatchDesc d;

        if ( ndims == 1 || lo.n == 1 )
        {
            int64_t first = offset( lo ), last = offset( hi );
            d = detail::make_linear_desc( pe, first, last - first + 1, nelems );

            if ( o )
            {
                d.odims[0] = o->dims.v[0];
                d.ostart[0] = o->starts.v[0];
            }
        }
        else
        {
            int64_t sub[RMACXX_MAX_DIMS];
            for ( int k = 0; k < ndims; k++ ) sub[k] = hi.v[k] - lo.v[k] + 1;
            d = detail::make_box_desc( pe, ndims, dims, lo.v, sub,
                                       o ? o->dims.v : nullptr, o ? o->starts.v : nullptr );
        }

        d.normalize();
        detail::exec_patch<T, S>( d, k, op, base, obuf, operand );
    }
};

// ------------------------------------------------------------------------
// global view: global arithmetic coordinates, patches may span PEs
// ------------------------------------------------------------------------
template <typename T>
struct DeviceWindow<T, GLOBAL_VIEW>
{
    T* base;            // symmetric address of the window
    int ndims;
    int me, npes;
    const int* rlo;     // [npes x ndims] inclusive lower bounds (device memory)
    const int* rhi;     // [npes x ndims] inclusive upper bounds (device memory)

    __device__ int rank() const { return me; }
    __device__ int size() const { return npes; }
    __device__ T* local() const { return base; }
    __device__ T* ptr( int pe ) const { return static_cast<T*>( nvshmem_ptr( base, pe ) ); }

    // owner PE of a global coordinate (-1 if none) and element offset in
    // the owner's local array
    __device__ int owner( Coord const& c, int64_t& off ) const
    {
        for ( int p = 0; p < npes; p++ )
        {
            bool in = true;

            for ( int k = 0; k < ndims && in; k++ )
                in = ( c.v[k] >= rlo[p*ndims+k] && c.v[k] <= rhi[p*ndims+k] );

            if ( in )
            {
                off = 0;
                for ( int k = 0; k < ndims; k++ )
                    off = off * ( rhi[p*ndims+k] - rlo[p*ndims+k] + 1 ) + ( c.v[k] - rlo[p*ndims+k] );
                return p;
            }
        }

        return -1;
    }

    // my block of the global array
    __device__ void local_range( Coord& lo, Coord& hi ) const
    {
        lo.n = hi.n = ndims;
        for ( int k = 0; k < ndims; k++ )
        {
            lo.v[k] = rlo[me*ndims+k];
            hi.v[k] = rhi[me*ndims+k];
        }
    }

    // ---- element-wise (coordinate must be owned by some PE) ----------
    __device__ void put( Coord const& at, T val ) const { int64_t o; int p = owner( at, o ); if ( p >= 0 ) detail::scalar_p( base + o, val, p ); }
    __device__ T get( Coord const& at ) const { int64_t o; int p = owner( at, o ); return ( p >= 0 ) ? detail::scalar_g( base + o, p ) : T(); }
    __device__ void atomic_put( Coord const& at, T val ) const { int64_t o; int p = owner( at, o ); if ( p >= 0 ) detail::amo_set( base + o, val, p ); }
    __device__ T atomic_get( Coord const& at ) const { int64_t o; int p = owner( at, o ); return ( p >= 0 ) ? detail::amo_fetch( base + o, p ) : T(); }
    __device__ void acc( Coord const& at, T val, Op op = SUM ) const { int64_t o; int p = owner( at, o ); if ( p >= 0 ) detail::amo_op( base + o, val, op.op, p ); }
    __device__ T fetch_op( Coord const& at, T val, Op op = SUM ) const { int64_t o; int p = owner( at, o ); return ( p >= 0 ) ? detail::amo_fetch_op( base + o, val, op.op, p ) : T(); }
    __device__ T cas( Coord const& at, T cmp, T val ) const { int64_t o; int p = owner( at, o ); return ( p >= 0 ) ? detail::amo_cas( base + o, cmp, val, p ) : T(); }

    // ---- bulk: global box [lo, hi] (inclusive) -----------------------
    template <Scope S = Scope::THREAD>
    __device__ void put_nbi( Coord const& lo, Coord const& hi, const T* src ) const { xfer_<S>( detail::Xfer::PUT, OpKind::NONE, lo, hi, const_cast<T*>( src ), nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void get_nbi( Coord const& lo, Coord const& hi, T* dst ) const { xfer_<S>( detail::Xfer::GET, OpKind::NONE, lo, hi, dst, nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void acc_nbi( Coord const& lo, Coord const& hi, const T* src, Op op = SUM ) const { xfer_<S>( detail::Xfer::ACC, op.op, lo, hi, const_cast<T*>( src ), nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void atomic_put_nbi( Coord const& lo, Coord const& hi, const T* src ) const { xfer_<S>( detail::Xfer::APUT, OpKind::NONE, lo, hi, const_cast<T*>( src ), nullptr, nullptr ); }
    template <Scope S = Scope::THREAD>
    __device__ void atomic_get_nbi( Coord const& lo, Coord const& hi, T* dst ) const { xfer_<S>( detail::Xfer::AGET, OpKind::NONE, lo, hi, dst, nullptr, nullptr ); }

    template <Scope S = Scope::THREAD>
    __device__ void put( Coord const& lo, Coord const& hi, const T* src ) const { put_nbi<S>( lo, hi, src ); detail::group_quiet<S>(); }
    template <Scope S = Scope::THREAD>
    __device__ void get( Coord const& lo, Coord const& hi, T* dst ) const { get_nbi<S>( lo, hi, dst ); detail::group_quiet<S>(); }
    template <Scope S = Scope::THREAD>
    __device__ void acc( Coord const& lo, Coord const& hi, const T* src, Op op = SUM ) const { acc_nbi<S>( lo, hi, src, op ); detail::group_quiet<S>(); }
    template <Scope S = Scope::THREAD>
    __device__ void gacc( Coord const& lo, Coord const& hi, const T* operand, T* result, Op op = SUM ) const
    { xfer_<S>( detail::Xfer::GACC, op.op, lo, hi, result, operand, nullptr ); detail::group_quiet<S>(); }

    template <Scope S = Scope::THREAD>
    __device__ void put_nbi( Coord const& lo, Coord const& hi, Origin<T> const& o ) const { xfer_<S>( detail::Xfer::PUT, OpKind::NONE, lo, hi, o.ptr, nullptr, &o ); }
    template <Scope S = Scope::THREAD>
    __device__ void get_nbi( Coord const& lo, Coord const& hi, Origin<T> const& o ) const { xfer_<S>( detail::Xfer::GET, OpKind::NONE, lo, hi, o.ptr, nullptr, &o ); }
    template <Scope S = Scope::THREAD>
    __device__ void acc_nbi( Coord const& lo, Coord const& hi, Origin<T> const& o, Op op = SUM ) const { xfer_<S>( detail::Xfer::ACC, op.op, lo, hi, o.ptr, nullptr, &o ); }

    __device__ void quiet() const { nvshmem_quiet(); }
    __device__ void fence() const { nvshmem_fence(); }

    template <Scope S>
    __device__ void xfer_( detail::Xfer k, OpKind op, Coord const& lo, Coord const& hi,
                           T* obuf, const T* operand, Origin<T> const* o ) const
    {
        for ( int p = 0; p < npes; p++ )
        {
            detail::PatchDesc d;

            if ( detail::make_global_piece( d, p, ndims, rlo, rhi, lo.v, hi.v,
                                            o ? o->dims.v : nullptr, o ? o->starts.v : nullptr ) )
            {
                d.normalize();
                detail::exec_patch<T, S>( d, k, op, base, obuf, operand );
            }
        }
    }
};
