#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    int lo, hi;

    // create window
    if (rank == 0) // process #0
    { 
        lo = 0;
        hi = 3;
    }
    else if (rank == 1) // process #1
    {
        lo = 4;
        hi = 11;
    }
    else if (rank == 2) // process #2
    {
        lo = 12;
        hi = 14;
    }
    else // process #3
    {
        lo = 15;
        hi = 17;
    }    

    // create window
    rmacxx::Window<int,GLOBAL_VIEW> win({lo}, {hi});

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
    std::vector<int> data(11);
    for(int i = 0; i < 11; i++)
        data[i] = 3;

    win({3},{13}) << data.data();
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
