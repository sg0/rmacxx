#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(1), hi(1);

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0; 
        hi[0] = 5;
    }
    else // process #1
    {   
        lo[0] = 6; 
        hi[0] = 9;
    }
           
    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");
   
    std::vector<int> vals(5);
    for (int i = 0; i < 5; i++)
        vals[i] = 3;

    // put
    win({4},{8}) << vals.data();
    
    win.flush();

    int nums[5];
    win({4},{8}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 5; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
