#include "rmacxx.hpp"
#include <list>
#include <cassert>

int main(int argc, char *argv[])
{

	int rank;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    double t0 = MPI_Wtime();

    // USER CONFIGURABLE VALUES
    std::vector<int> lo(2), hi(2); // size of each process, inclusive coordinates
    if (rank == 0) { // process #0
        lo[0] = 0; lo[1] = 0;
        hi[0] = 5; hi[1] = 5;
    }
    else if (rank == 1) { // process #1
        lo[0] = 6; lo[1] = 0;
        hi[0] = 9; hi[1] = 5;
    }
    else if (rank == 2) { // process #2
        lo[0] = 0; lo[1] = 6;
        hi[0] = 2; hi[1] = 9;
    }
    else if (rank == 3) { // process #3     creates a 3-wide gap in the x dimension
        lo[0] = 6; lo[1] = 6;
        hi[0] = 9; hi[1] = 9;
    }
    // else if (rank == 4) { // process #4     if not commented out, fails test (intended)
    //    lo[0] = 3; lo[1] = 6;
    //    hi[0] = 9; hi[1] = 9;
    // } 

    bool exception_was_thrown = false;
    double t1;
    try {
        rmacxx::Window<int, GLOBAL_VIEW> win(lo, hi); // Exception should be thrown here
        t1 = MPI_Wtime();     
    } catch(int construction_error) { //if throws exception, then should pass
        t1 = MPI_Wtime();     
        MPI_Finalize();
       exception_was_thrown = true;
    }
    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;
    assert(exception_was_thrown);
}
