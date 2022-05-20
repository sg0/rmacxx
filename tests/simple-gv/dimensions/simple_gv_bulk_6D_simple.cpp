#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> dims(6), pgrid(6);

    // create window
    if (rank == 0) // process #0
    { 
        dims[0] = 2; dims[1] = 2; dims[2] = 2; dims[3] = 2; dims[4] = 2; dims[5] = 2;
        pgrid[0] = 0; pgrid[1] = 0; pgrid[2] = 0; pgrid[3] = 0; pgrid[4] = 0; pgrid[5] = 0;
    }
    else // process #1
    {
        dims[0] = 2; dims[1] = 2; dims[2] = 2; dims[3] = 2; dims[4] = 2; dims[5] = 2;
        pgrid[0] = 0; pgrid[1] = 1; pgrid[2] = 0; pgrid[3] = 0; pgrid[4] = 0; pgrid[5] = 0;
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
    std::vector<int> data(64);
    for(int i = 0; i < 64; i++)
        data[i] = 3;

    win({0,1},{1,2}) << data.data();
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
