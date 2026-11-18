// Driver for running an RMACXX test on the CPU emulator (testing only).
// The test translation unit is compiled with -Dmain=rmacxx_test_main and
// linked with this file; EMU_NP (or argv[1] of the form -np=N) selects the
// number of PEs.
#include <cstdlib>
#include <cstring>
#include <string>

#include "emu_core.hpp"

int rmacxx_test_main( int argc, char** argv );

int main( int argc, char** argv )
{
    int np = 2;

    if ( const char* e = std::getenv( "EMU_NP" ) )
        np = std::atoi( e );

    if ( argc > 1 && std::strncmp( argv[1], "-np=", 4 ) == 0 )
        np = std::atoi( argv[1] + 4 );

    return emu::run( np, [&]( int ) { return rmacxx_test_main( argc, argv ); } );
}
