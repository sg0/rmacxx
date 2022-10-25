#define RMACXX_SUBARRAY_USE_END_COORDINATES
#define DEBUG_CHECK_LIMITS
#include "rmacxx.hpp"
#include <cassert>

// This test should remain commented out until subarray bounds checking is properly implemented
int main(int argc, char *argv[])
{

    // // ===== TESTS =====

    // // STARTS ENDS
    // // 1) second coords less than first


    // // create processes
    // int rank;
    // MPI_Init( &argc, &argv );
    // MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    // double t0 = MPI_Wtime();

    // std::vector<int> lo(2), hi(2); // size of each process, inclusive coordinates
    // if (rank == 0) // process #0
    // { 
    //     lo[0] = 0; lo[1] = 0;
    //     hi[0] = 1; hi[1] = 3;
    // }
    // else if (rank == 1) // process #1
    // {
    //     lo[0] = 2; lo[1] = 0;
    //     hi[0] = 3; hi[1] = 3;
    // }

    // // create Window
    // rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

    // if (win.size() != 2)
    // {
    //     std::cout << "Number of processes should be exactly 2. Aborting..." << std::endl;
    //     MPI_Abort(MPI_COMM_WORLD, -99);
    // }

    // // print data ranges per process
    // win.print_ranges();

    // // fill window buffer
    // win.fill(1);

    // // print shape
    // win.print("Current...");

    // // create local buffers
    // std::vector<int> local_buffer0(4); //2x2, 4 tiles
    // for (int i = 0; i < 4; i++) {
    //     local_buffer0[i] = 3;
    // }
    // std::vector<int> local_buffer1(6); //2x3, 6 tiles
    // for (int i = 0; i < 6; i++) {
    //     local_buffer1[i] = 3;
    // }


    // bool error = false;


    // try {
    //     // TEST #1                                      starts  ends                                            
    //     rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub0({2, 2},{1, 2});
    // } catch(int error) { //if throws exception, then should pass
    //     error = true;
    // }

    // win.wfree();

    
    // MPI_Finalize();

    // // test should have thrown error so this should be true
    // assert(error);
}