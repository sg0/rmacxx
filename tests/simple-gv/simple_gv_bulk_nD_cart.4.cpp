#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    // int rank;

    // MPI_Init( &argc, &argv );
    // MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    // std::vector<int> dims(2), pgrid(2);

    // // first dimension is height, then width

    // // create window
    // if (rank == 0) // process #0
    // { 
    //     dims[0] = 3; dims[1] = 4;
    //     pgrid[0] = 0; pgrid[1] = 0;
    // }
    // else if (rank == 1) // process #1
    // {
    //     dims[0] = 5; dims[1] = 8;
    //     pgrid[0] = 0; pgrid[1] = 1;
    // }
    // else if (rank == 2) // process #2
    // { 
    //     dims[0] = 5; dims[1] = 4;
    //     pgrid[0] = 1; pgrid[1] = 0;
    // }
    // else // process #3
    // {
    //     dims[0] = 3; dims[1] = 8;
    //     pgrid[0] = 1; pgrid[1] = 1;
    // }    

    // rmacxx::Window<int,GLOBAL_VIEW> win(dims, pgrid);

    // if (win.size() != 4)
    // {
    //     std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
    //     MPI_Abort(MPI_COMM_WORLD, -99);
    // }

    // // print data ranges per process
    // win.print_ranges();

    // // fill window buffer
    // win.fill(1);

    // // print shape
    // win.print("Current...");
    
    // // put
    // std::vector<int> data(8);
    // for(int i = 0; i < data.size(); i++)
    //     data[i] = 3;

    // win({2,3},{6,5}) << data.data();
    
    // win.flush();
    
    // win.print("After put...");

    // win.barrier();
    
    // win.wfree();

    // MPI_Finalize();

    return 0;
}
