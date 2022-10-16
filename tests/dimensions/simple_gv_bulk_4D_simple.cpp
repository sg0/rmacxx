#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> los(4), his(4);

    // create window
    if (rank == 0) // process #0
    { 
        los[0] = 0; los[1] = 0; los[2] = 0; los[3] = 0;
        his[0] = 1; his[1] = 3; his[2] = 3; his[3] = 3;
    }
    else // process #1
    {
        los[0] = 2; los[1] = 0; los[2] = 0; los[3] = 0;
        his[0] = 3; his[1] = 3; his[2] = 3; his[3] = 3;
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
    std::vector<int> data(16);
    for(int i = 0; i < 16; i++)
        data[i] = 3;

    win({1,1,1,1},{2,2,2,2}) << data.data(); //inner cube is 2x2x2x2, 16 total
    
    win.flush();

    int nums[16];
    win({1,1,1,1},{2,2,2,2}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 16; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
