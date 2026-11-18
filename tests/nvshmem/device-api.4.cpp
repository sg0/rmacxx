// Device-initiated communication through Window::device_view().
// Global view: an 8x8 array distributed 2x2 over 4 PEs; patches written
// from a kernel span several PEs. Local view: strided origin, accumulate,
// fetch-and-op and compare-and-swap from device code.
//
// Kernels are launched with RMACXX_LAUNCH so that the same source also
// runs on the CPU emulator; with nvcc it expands to kernel<<<...>>>.
#include "rmacxx.hpp"
#include <cassert>

using GV = rmacxx::DeviceWindow<int, GLOBAL_VIEW>;
using LV = rmacxx::DeviceWindow<double, LOCAL_VIEW>;
using rmacxx::Scope;

// PE p writes the value p+1 into global rows 2p..2p+1 (all 8 columns), as
// a block-collective put
__global__ void gv_write( GV dw, const int* src )
{
    const int p = dw.rank();
    dw.put<Scope::BLOCK>( { 2 * p, 0 }, { 2 * p + 1, 7 }, src );
}

// every PE atomically adds 100 to element {7,7}
__global__ void gv_acc( GV dw )
{
    if ( threadIdx.x == 0 && blockIdx.x == 0 )
    {
        dw.acc( { 7, 7 }, 100, SUM );
        dw.quiet();
    }
}

// warp-collective get of the whole global array
__global__ void gv_read( GV dw, int* dst )
{
    if ( threadIdx.x < 32 )
        dw.get<Scope::WARP>( { 0, 0 }, { 7, 7 }, dst );
}

// local view: put a 2x3 patch taken out of a strided 4x5 origin buffer
// (starting at {1,1}) into the next PE, then fetch-and-add / CAS on PE 0
__global__ void lv_ops( LV dw, const double* src45, double* out )
{
    if ( threadIdx.x != 0 || blockIdx.x != 0 )
        return;

    const int next = ( dw.rank() + 1 ) % dw.size();
    rmacxx::Origin<double> o( src45, { 4, 5 }, { 1, 1 } );
    dw.put_nbi( next, { 0, 0 }, { 1, 2 }, o );
    dw.quiet();

    out[0] = dw.fetch_op( 0, { 3, 3 }, 1.5, SUM );
    out[1] = dw.cas( 0, { 3, 4 }, 0.0, 42.0 + dw.rank() );
    dw.acc( 0, { 2, 2 }, 2.0, MAX );
    dw.quiet();
}

int main( int argc, char* argv[] )
{
    rmacxx::init( &argc, &argv );
    int rank;
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );

    // ---------------- global view ----------------
    std::vector<int> lo( 2 ), hi( 2 );
    lo[0] = ( rank / 2 ) * 4; hi[0] = lo[0] + 3;
    lo[1] = ( rank % 2 ) * 4; hi[1] = lo[1] + 3;
    rmacxx::Window<int, GLOBAL_VIEW> gw( lo, hi );

    if ( gw.size() != 4 )
    {
        std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
        MPI_Abort( MPI_COMM_WORLD, -99 );
    }

    gw.fill( 0 );

    int *src, *dst;
    std::vector<int> h( 16, rank + 1 ), all( 64 );
    RMACXX_CUDA_CHECK( cudaMalloc( &src, 16 * sizeof( int ) ) );
    RMACXX_CUDA_CHECK( cudaMalloc( &dst, 64 * sizeof( int ) ) );
    rmacxx::register_buffer( src, 16 * sizeof( int ) );   // needed over IB
    rmacxx::register_buffer( dst, 64 * sizeof( int ) );
    RMACXX_CUDA_CHECK( cudaMemcpy( src, h.data(), 16 * sizeof( int ), cudaMemcpyHostToDevice ) );

    RMACXX_LAUNCH( gv_write, 1, 128, gw.stream(), gw.device_view(), src );
    gw.barrier();   // completes device-initiated operations on all PEs
    RMACXX_LAUNCH( gv_acc, 1, 32, gw.stream(), gw.device_view() );
    gw.barrier();
    RMACXX_LAUNCH( gv_read, 1, 32, gw.stream(), gw.device_view(), dst );
    RMACXX_CUDA_CHECK( cudaStreamSynchronize( gw.stream() ) );
    RMACXX_CUDA_CHECK( cudaMemcpy( all.data(), dst, 64 * sizeof( int ), cudaMemcpyDeviceToHost ) );

    for ( int i = 0; i < 8; i++ )
        for ( int j = 0; j < 8; j++ )
        {
            int expect = i / 2 + 1 + ( ( i == 7 && j == 7 ) ? 400 : 0 );
            assert( all[i * 8 + j] == expect );
        }

    // the host API sees the same data
    int e77;
    gw( { 7, 7 } ) >> e77;
    assert( e77 == 404 );

    gw.barrier();
    rmacxx::unregister_buffer( src );
    rmacxx::unregister_buffer( dst );
    cudaFree( src );
    cudaFree( dst );
    gw.wfree();

    // ---------------- local view ----------------
    rmacxx::Window<double, LOCAL_VIEW> lw( { 4, 5 } );
    lw.fill( 0.0 );

    double *s45, *out;
    std::vector<double> hs( 20 );
    for ( int i = 0; i < 20; i++ ) hs[i] = 1000 * rank + i;
    RMACXX_CUDA_CHECK( cudaMalloc( &s45, 20 * sizeof( double ) ) );
    RMACXX_CUDA_CHECK( cudaMalloc( &out, 2 * sizeof( double ) ) );
    rmacxx::register_buffer( s45, 20 * sizeof( double ) );
    rmacxx::register_buffer( out, 2 * sizeof( double ) );
    RMACXX_CUDA_CHECK( cudaMemcpy( s45, hs.data(), 20 * sizeof( double ), cudaMemcpyHostToDevice ) );

    RMACXX_LAUNCH( lv_ops, 1, 32, lw.stream(), lw.device_view(), s45, out );
    lw.barrier();

    double ho[2];
    RMACXX_CUDA_CHECK( cudaMemcpy( ho, out, 2 * sizeof( double ), cudaMemcpyDeviceToHost ) );
    assert( ho[0] >= 0.0 && ho[0] <= 4.5 );          // one of 0, 1.5, 3, 4.5
    assert( ho[1] == 0.0 || ho[1] >= 42.0 );

    // my window holds the patch written by the previous PE: its origin
    // rows 1..2, cols 1..3 of a 4x5 array
    const int prev = ( rank + 3 ) % 4;
    double mine[6];
    lw( rank, { 0, 0 }, { 1, 2 } ) >> mine;
    lw.flush();
    for ( int i = 0; i < 2; i++ )
        for ( int j = 0; j < 3; j++ )
            assert( mine[i * 3 + j] == 1000 * prev + ( i + 1 ) * 5 + ( j + 1 ) );

    double v33, v34, v22;
    lw( 0, { 3, 3 } ) >> v33;
    lw( 0, { 3, 4 } ) >> v34;
    lw( 0, { 2, 2 } ) >> v22;
    lw.flush();
    assert( v33 == 6.0 );
    assert( v34 >= 42.0 && v34 <= 45.0 );
    assert( v22 == 2.0 );

    lw.barrier();
    rmacxx::unregister_buffer( s45 );
    rmacxx::unregister_buffer( out );
    cudaFree( s45 );
    cudaFree( out );
    lw.wfree();

    if ( rank == 0 )
        std::cout << "device API: OK" << std::endl;

    rmacxx::finalize();
    return 0;
}
