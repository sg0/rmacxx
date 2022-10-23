#include "rmacxx.hpp"
#include <cassert>

#define RMACXX_SUBARRAY_USE_END_COORDINATES
// This test should remain commented out until subarray bounds checking is properly implemented
int main(int argc, char *argv[]){
}
// int main(int argc, char *argv[])
// {

//     // ===== TESTS =====

//     // STARTS SIZES
//     // 1) first coords negative
//     // 2) second coords negative

//     // WINDOW
//     // 3) place out-of-bounds subarray into window (starts in negative coords)
//     // 4) place out-of-bounds subarray into window (starts outside of window)
//     // 5) place out-of-bounds subarray into window (extends past border)
//     // 6) place subarray into invalid window coordinates (second coords larger than first)


//     // create processes
//     int rank;
//     MPI_Init( &argc, &argv );
//     MPI_Comm_rank( MPI_COMM_WORLD, &rank );
//     double t0 = MPI_Wtime();

//     std::vector<int> lo(2), hi(2); // size of each process, inclusive coordinates
//     if (rank == 0) // process #0
//     { 
//         lo[0] = 0; lo[1] = 0;
//         hi[0] = 1; hi[1] = 3;
//     }
//     else if (rank == 1) // process #1
//     {
//         lo[0] = 2; lo[1] = 0;
//         hi[0] = 3; hi[1] = 3;
//     }

//     // create Window
//     rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

//     if (win.size() != 2)
//     {
//         std::cout << "Number of processes should be exactly 2. Aborting..." << std::endl;
//         MPI_Abort(MPI_COMM_WORLD, -99);
//     }

//     // print data ranges per process
//     win.print_ranges();

//     // fill window buffer
//     win.fill(1);

//     // print shape
//     win.print("Current...");

//     // create local buffers
//     std::vector<int> local_buffer0(4); //2x2, 4 tiles
//     for (int i = 0; i < 4; i++) {
//         local_buffer0[i] = 3;
//     }
//     std::vector<int> local_buffer1(6); //2x3, 6 tiles
//     for (int i = 0; i < 6; i++) {
//         local_buffer1[i] = 3;
//     }


//     // create the assertion vector
//     std::vector<bool> errors(6);
//     for (int i = 0; i < 6; i++) {
//         errors[i] = false;
//     }

    
//     try {
//         // TEST #1                                      starts  sizes                                            
//         rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub0({-1, 0},{1, 2});
//     } catch(int error) { //if throws exception, then should pass
//        errors[0] = true;
//     }

    
//     try {
//         // TEST #2                                      starts  sizes 
//         rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub1({0, 0},{-1, 2});
//     } catch(int error) { //if throws exception, then should pass
//        errors[1] = true;
//     }
    
//     // legitimate subarrays                         starts  sizes
//     rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub2({0, 0},{2, 3}); //6 total tiles
//     rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub3({0, 0},{2, 2}); //4 total tiles
    

//     //sub3(local_buffer0.data());

//     try {
//         // TEST #3
//         win({-1, 0},{0, 0}) << sub3(local_buffer0.data());
//     } catch(int error) { //if throws exception, then should pass
//        errors[2] = true;
//     }
//     try {
//          // TEST #4
//         win({10, 0},{10, 1}) << sub3(local_buffer0.data());
//     } catch(int error) { //if throws exception, then should pass
//        errors[3] = true;
//     }
//     try {
//         // TEST #5
//         win({3, 0},{4, 1}) << sub3(local_buffer0.data());
//     } catch(int error) { //if throws exception, then should pass
//        errors[4] = true;
//     }
//     try {
//         // TEST #6
//         win({10, 0},{0, 1}) << sub3(local_buffer0.data());
//     } catch(int error) { //if throws exception, then should pass
//        errors[5] = true;
//     }

//     win.flush();
//     MPI_Finalize();

//     // all of the tests should have thrown an error, so all of these should be true
//     for (int i = 0; i < 6; i++) {
//         assert(errors[i]);
//     }


// }