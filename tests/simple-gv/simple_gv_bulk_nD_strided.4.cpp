#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(2), hi(2);

    // first dimension is height, then width

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0;
        hi[0] = 2; hi[1] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 0; lo[1] = 4;
        hi[0] = 4; hi[1] = 11;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 3; lo[1] = 0;
        hi[0] = 7; hi[1] = 3;
    }
    else // process #3
    {
        lo[0] = 5; lo[1] = 4;
        hi[0] = 7; hi[1] = 11;
    }    

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

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
    std::vector<int> data(9*8);
    for(int i = 0; i < 72; i++)
        data[i] = 0;

    // mark subarray parts
    for(int i = 0; i < 9; i++)
    {
        if ((i >= 2) && (i <= 6)) 
        {
            for(int k = 0; k < 3; k++)
                data[i*8+k+2] = 3;
        }
    }

    // print local buffer
    if (rank == 0)
    {
        std::cout << "Local buffer: " << std::endl;
        for(int i = 0; i < 9; i++)
        {
            for(int k = 0; k < 8; k++)
            {
                std::cout << data[i*8+k] << " ";
            }
            std::cout << std::endl;
        }
    }

    // create subarray type for global transfer
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({2,2},{9,8});

    // put
    win({2,3},{6,5}) << sub(data.data());
    
    win.flush();

    int nums[15];
    win({2,3},{6,5}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();
    

    data.clear();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 15; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
