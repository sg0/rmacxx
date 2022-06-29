#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(9), hi(9);

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 1; hi[1] = 1; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 2; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 3; hi[1] = 1; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 2; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 1; hi[1] = 3; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }
    else // process #3
    {
        lo[0] = 2; lo[1] = 2; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 3; hi[1] = 3; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }   

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi); //window is a 4-wide hypercube

    if (win.size() != 4)
    {
        std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");

    // local buffer
    std::vector<int> data(19683); //3^9
    for(int i = 0; i < 19683; i++) {
        data[i] = 0;
    }
    
    for(int a = 0; a < 2; a++)
    {
        for(int b = 0; b < 2; b++)
        {
            for(int c = 0; c < 2; c++)
            {
                for(int d = 0; d < 2; d++)
                {
                    for(int e = 0; e < 2; e++)
                    {
                        for(int f = 0; f < 2; f++)
                        {
                            for(int g = 0; g < 2; g++)
                            {
                                for(int h = 0; h < 2; h++)
                                {
                                    for(int i = 0; i < 2; i++)
                                    {
                                        data[(i + h*3 + g*9 + f*27 + e*81 + d*243 + c*729 + b*2187 + a*6561)] = 3; //range of 0 to 19682 = 3^9 - 1
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // create subarray type for global transfer     //starts            //sizes
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({0,0,0,0,0,0,0,0,0},{2,2,2,2,2,2,2,2,2}); //extract a 2-wide hypercube
    //rmacxx::RMACXX_Subarray_t<int, GLOBAL_VIEW> sub({2,2}, {4,6});

    //window is inclusive
    //subarray is inclusive

    // put
    win({1,1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2,2}) << sub(data.data()); //put the 2-wide hypercube subarray into the center of the 4-wide hypercube
    
    win.flush();
    
    win.print("After put...");

    win.barrier(); //processes wait here at the barrier, .barrier() is a collective
    
    win.wfree();
    
    data.clear();

    int nums[512];
    win({1,1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2,2}) >> nums;

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 512; i++) {
            all_threes = all_threes && nums[i] == 3;
            std::cout<<nums[i];
        }
        assert(all_threes);
        std::cout<<std::endl;
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
