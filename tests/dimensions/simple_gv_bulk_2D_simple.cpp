#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> dims(2), pgrid(2);

    // create window
    if (rank == 0) // process #0
    { 
        dims[0] = 0; dims[1] = 0;
        pgrid[0] = 1; pgrid[1] = 3;
    }
    else // process #1
    {
        dims[0] = 2; dims[1] = 0;
        pgrid[0] = 3; pgrid[1] = 3;
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
    std::vector<int> data(4);
    for(int i = 0; i < 4; i++) {
        data[i] = 3;
    }

<<<<<<< HEAD:tests/dimensions/simple_gv_bulk_2D_simple.cpp
    win({1,1},{2,2}) << data.data(); //inner cube is 2x2, 4 total
    
=======
    // win({2,3},{6,5}) << data.data();
    win({0,0},{4,6}) << data.data();
>>>>>>> db47f00 (Update build styem):tests/simple-gv/simple_gv_bulk_nD_cart.4.cpp
    win.flush();

    int nums[4];
    win({1,1},{2,2}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();
    
    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 4; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
