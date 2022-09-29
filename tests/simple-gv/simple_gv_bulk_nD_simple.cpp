#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> los(2), his(2);

    // create window
    if (rank == 0) // process #0
    { 
        los[0] = 0; los[1] = 0;
        his[0] = 1; his[1] = 0;
    }
    else // process #1
    {
        los[0] = 0; los[1] = 1;
        his[0] = 1; his[1] = 1;
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
    std::vector<int> data(4);
    for(int i = 0; i < 4; i++)
        data[i] = 3;

    win({0,0},{1,1}) << data.data();
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
