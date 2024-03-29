#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> los(2), his(2);

    // first dimension is height, then width

    // create window
    if (rank == 0) // process #0
    { 
        los[0] = 0; los[1] = 0;
        his[0] = 0; his[1] = 1;
    }
    else if (rank == 1) // process #1
    {
        los[0] = 0; los[1] = 2;
        his[0] = 0; his[1] = 3;
    }
    else if (rank == 2) // process #2
    { 
        los[0] = 1; los[1] = 0;
        his[0] = 1; his[1] = 1;
    }
    else // process #3
    {
        los[0] = 1; los[1] = 2;
        his[0] = 1; his[1] = 3;
    }    

    rmacxx::Window<int,GLOBAL_VIEW> win(los, his);

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
    std::vector<int> data(8);
    for(int i = 0; i < data.size(); i++)
        data[i] = 3;

    win({0,0},{1,3}) << data.data();
    
    win.flush();

    int nums[8];
    win({0,0},{1,3}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 8; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
