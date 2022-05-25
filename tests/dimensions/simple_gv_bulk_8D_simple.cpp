#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> dims(8), pgrid(8);

    // create window
    if (rank == 0) // process #0
    { 
        dims[0] = 0; dims[1] = 0; dims[2] = 0; dims[3] = 0; dims[4] = 0; dims[5] = 0; dims[6] = 0; dims[7] = 0;
        pgrid[0] = 1; pgrid[1] = 1; pgrid[2] = 1; pgrid[3] = 1; pgrid[4] = 1; pgrid[5] = 1; pgrid[6] = 1; pgrid[7] = 1;
    }
    else // process #1
    {
        dims[0] = 2; dims[1] = 2; dims[2] = 2; dims[3] = 2; dims[4] = 2; dims[5] = 2; dims[6] = 2; dims[7] = 2;
        pgrid[0] = 3; pgrid[1] = 3; pgrid[2] = 3; pgrid[3] = 3; pgrid[4] = 3; pgrid[5] = 3; pgrid[6] = 3; pgrid[7] = 3;
    }
           
    rmacxx::Window<int,GLOBAL_VIEW> win(dims, pgrid);

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
    std::vector<int> data(6561);
    for(int i = 0; i < 6561; i++)
        data[i] = 3;

    win({0,0,0,0,0,0,0,0},{2,2,2,2,2,2,2,2}) << data.data();
    
    win.flush();

    int nums[256];
    win({1,1,1,1,1,1,1,1},{2,2,2,2,2,2,2,2}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 256; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
