#include "rmacxx.hpp"

// irregular data distribution

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(2), hi(2);

    // first dimension is height, then width

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0;
        hi[0] = 10; hi[1] = 10;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 0; lo[1] = 10;
        hi[0] = 10; hi[1] = 20;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 20;
        hi[0] = 10; hi[1] = 30;
    }
    else // process #3
    {
        lo[0] = 0; lo[1] = 30;
        hi[0] = 10; hi[1] = 40;
    }    

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

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
    std::vector<int> data(4*31);
    for(int i = 0; i < 124; i++)
        data[i] = 3;

    win({5,5},{8,35}) << data.data();
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    data.clear();
    win.wfree();

    MPI_Finalize();

    return 0;
}
