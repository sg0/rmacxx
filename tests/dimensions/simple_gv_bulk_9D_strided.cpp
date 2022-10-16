#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    // int rank;

    // MPI_Init( &argc, &argv );
    // MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    // std::vector<int> lo(9), hi(9);

    // // first dimension is height, then width

    // // create window
    // if (rank == 0) // process #0
    // { 
    //     lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
    //     hi[0] = 1; hi[1] = 1; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    // }
    // else if (rank == 1) // process #1
    // {
    //     lo[0] = 2; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
    //     hi[0] = 3; hi[1] = 1; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    // }
    // else if (rank == 2) // process #2
    // {
    //     lo[0] = 0; lo[1] = 2; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
    //     hi[0] = 1; hi[1] = 3; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    // }
    // else // process #3
    // {
    //     lo[0] = 2; lo[1] = 2; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
    //     hi[0] = 3; hi[1] = 3; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    // }

    // rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

    // if (win.size() != 4)
    // {
    //     std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
    //     MPI_Abort(MPI_COMM_WORLD, -99);
    // }

    // // print data ranges per process
    // win.print_ranges();

    // // fill window buffer
    // win.fill(1);

    // // print shape
    // win.print("Current...");

    // // local buffer
    // std::vector<int> data(262144); //4^9
    // for(int i = 0; i < 262144; i++) {
    //     data[i] = 0;
    // }

    // for(int a = 1; a < 3; a++) 
    // {
    //     for(int b = 1; b < 3; b++)
    //     {
    //         for(int c = 1; c < 3; c++)
    //         {
    //             for(int d = 1; d < 3; d++)
    //             {
    //                 for(int e = 1; e < 3; e++)
    //                 {
    //                     for(int f = 1; f < 3; f++)
    //                     {
    //                         for(int g = 1; g < 3; g++)
    //                         {
    //                             for(int h = 1; h < 3; h++)
    //                             {
    //                                 for(int i = 1; i < 3; i++)
    //                                 {
    //                                     data[i + h*4 + g*16 + f*64 + e*256 + d*1024 + c*4096 + b*16384 + a*65536] = 3; //range of 0 to 262143 = 4^9 - 1
    //                                 }
    //                             }
    //                         }
    //                     }
    //                 }
    //             }
    //         }
    //     }
    // }

    // // put
    // win({0,0,0,0,0,0,0,0,0},{3,3,3,3,3,3,3,3,3}) << data.data();

    // int nums[512];
    // win({1,1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2,2}) >> nums;
    
    // win.flush();
    
    // win.print("After put...");

    // win.barrier();
    
    // win.wfree();
    
    // data.clear();

    // MPI_Finalize();

    // if (rank == 0) {
    //     bool all_threes = true;
    //     for (int i = 0; i < 512; i++) {
    //         all_threes = all_threes && nums[i] == 3;
    //     }
    //     assert(all_threes);
    //     std::cout<<"Pass"<<std::endl;
    // }

    // return 0;
}
