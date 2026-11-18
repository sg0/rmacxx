// Accumulate, get-accumulate, fetch-and-op, compare-and-swap and atomic
// put/get through the host API (local view), for several element types.
// All PEs hit the same target concurrently, so this also checks atomicity.
#include "rmacxx.hpp"
#include <cassert>
#include <cmath>

template <typename T, WinAtomicity A = ATOMIC_NONE>
void run( const char* name )
{
    rmacxx::Window<T, LOCAL_VIEW, NO_EXPR, NO_FLUSH, A> win( { 4, 6 } );
    const int np = win.size();
    const int me = win.rank();
    win.fill( T( 1 ) );

    // every PE adds 2 to a 3x4 patch of PE 0
    std::vector<T> two( 12, T( 2 ) );
    win( 0, { 1, 1 }, { 3, 4 }, SUM ) << two.data();
    win.flush();
    win.barrier();

    std::vector<T> got( 12 );
    win( 0, { 1, 1 }, { 3, 4 } ) >> got.data();
    win.flush();
    for ( T v : got ) assert( v == T( 1 + 2 * np ) );

    // MAX with the PE rank on one element of the last PE
    win( np - 1, { 0, 5 }, MAX ) << T( me + 10 );
    win.flush();
    win.barrier();
    T mx;
    win( np - 1, { 0, 5 } ) >> mx;
    win.flush();
    assert( mx == T( np - 1 + 10 ) );

    // fetch-and-add on element {0,0} of PE 0: the returned values must be
    // a permutation of 1, 2, ..., np
    T old;
    win( 0, { 0, 0 }, SUM, T( 1 ) ) >> old;
    win.flush();
    win.barrier();
    T fin;
    win( 0, { 0, 0 } ) >> fin;
    win.flush();
    assert( fin == T( 1 + np ) );
    assert( old >= T( 1 ) && old <= T( np ) );

    // compare-and-swap: only one PE wins on element {3,5} of PE 1 % np
    const int tgt = 1 % np;
    T prev;
    win( tgt, { 3, 5 }, T( 100 + me ), T( 1 ) ) >> prev;
    win.flush();
    win.barrier();
    T now;
    win( tgt, { 3, 5 } ) >> now;
    win.flush();
    assert( now >= T( 100 ) && now < T( 100 + np ) );
    assert( prev == T( 1 ) || prev == now );

    // get-accumulate (bulk): every PE adds its operand and gets the old values
    win.barrier();
    std::vector<T> opnd( 4, T( 1 ) ), res( 4 );
    win( 0, { 2, 0 }, { 2, 3 }, SUM, opnd.data() ) >> res.data();
    win.flush();
    win.barrier();
    std::vector<T> after( 4 );
    win( 0, { 2, 0 }, { 2, 3 } ) >> after.data();
    win.flush();
    // row 2, cols 0..3: col 0 is untouched by the first acc (1), cols 1..3 were 1+2*np
    assert( after[0] == T( 1 + np ) );
    assert( after[1] == T( 1 + 2 * np + np ) );

    win.barrier();
    if ( me == 0 )
        std::cout << name << ( A == ATOMIC_PUT_GET ? " (atomic put/get)" : "" ) << ": OK" << std::endl;
    win.wfree();
}

int main( int argc, char* argv[] )
{
    rmacxx::init( &argc, &argv );
    run<int>( "int" );
    run<long long>( "long long" );
    run<unsigned>( "unsigned" );
    run<float>( "float" );
    run<double>( "double" );
    run<short>( "short" );
    run<char>( "char" );
    run<double, ATOMIC_PUT_GET>( "double" );
    run<short, ATOMIC_PUT_GET>( "short" );
    rmacxx::finalize();
    return 0;
}
