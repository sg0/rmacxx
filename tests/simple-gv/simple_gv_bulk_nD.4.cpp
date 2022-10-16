#include "rmacxx.hpp"

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
    
    // put
    std::vector<int> data(3*5);
    for(int i = 0; i < 15; i++)
        data[i] = 3;

    // win({2,3},{6,5}) << data.data();
    win({0,0},{4,6}) << data.data();
    win.flush();

    int nums[15];
    win({2,3},{6,5}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

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
