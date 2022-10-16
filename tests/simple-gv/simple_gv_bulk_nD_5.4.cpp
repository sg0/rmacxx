#include "rmacxx.hpp"

// regular data distribution

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
        hi[0] = 3; hi[1] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 0; lo[1] = 4;
        hi[0] = 3; hi[1] = 9;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 4; lo[1] = 0;
        hi[0] = 7; hi[1] = 3;
    }
    else // process #3
    {
        lo[0] = 4; lo[1] = 4;
        hi[0] = 7; hi[1] = 9;
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
    
    // put
    std::vector<int> data(6*6);
    for(int i = 0; i < 36; i++)
        data[i] = 3;

    win({1,1},{6,6}) << data.data();
    
    win.flush();

    int nums[36];
    win({1,1},{6,6}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();
    data.clear();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 36; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
