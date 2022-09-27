#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> los(5), his(5);

    // create window
    if (rank == 0) // process #0
    { 
        los[0] = 0; los[1] = 0; los[2] = 0; los[3] = 0; los[4] = 0;
        his[0] = 1; his[1] = 3; his[2] = 3; his[3] = 3; his[4] = 3;
    }
    else // process #1
    {
        los[0] = 2; los[1] = 0; los[2] = 0; los[3] = 0; los[4] = 0;
        his[0] = 3; his[1] = 3; his[2] = 3; his[3] = 3; his[4] = 3;
    }
           
    rmacxx::Window<int,GLOBAL_VIEW> win(los, his);

    if (win.size() != 2)
    {
        std::cout << "Number of processes should be exactly 2. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");
    
    // put
    std::vector<int> data(32);
    for(int i = 0; i < 32; i++)
        data[i] = 3;

    win({1,1,1,1,1},{2,2,2,2,2}) << data.data(); //inner cube is 2x2x2x2x2, 32 total
    
    win.flush();

    int nums[32];
    win({1,1,1,1,1},{2,2,2,2,2}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 32; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
