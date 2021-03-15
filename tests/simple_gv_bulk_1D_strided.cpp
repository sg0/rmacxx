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
        hi[0] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 4;
        hi[0] = 11;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 12;
        hi[0] = 14;
    }
    else // process #3
    {
        lo[0] = 15;
        hi[0] = 17;
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
    std::vector<int> data(20);
    for(int i = 0; i < 20; i++)
        data[i] = 3;

    MPI_Datatype vect1d_t;
    MPI_Type_vector(3, 4, 4, MPI_INT, &vect1d_t);
    MPI_Type_commit(&vect1d_t);

    rmacxx::RMACXX_Local_t<int> rmx_vt(vect1d_t);

    win({3},{13}) << rmx_vt(data.data());
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
   
    MPI_Type_free(&vect1d_t);

    win.wfree();

    MPI_Finalize();

    return 0;
}
