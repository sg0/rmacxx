#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(1), hi(1);

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0;
        hi[0] = 0;
    }
    else // process #1
    {
        lo[0] = 1;
        hi[0] = 1;
    }
           
    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");
    
    // put
    win({0}) << 3;
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
