#include "rmacxx.hpp"

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
        pgrid[0] = 2; pgrid[1] = 2;
    }
    else // process #1
    {
        dims[0] = 0; dims[1] = 3;
        pgrid[0] = 2; pgrid[1] = 4;
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
    for(int i = 0; i < 4; i++)
        data[i] = 3;

    win({0,1},{1,2}) << data.data();
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
