#define DEBUG_CHECK_GAPS
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
    std::vector<int> lo(9), hi(9); // size of each process, inclusive coordinates
    if (rank == 0) { // process #0
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0; //covers the entire floor, 4 tall
        hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 9; hi[4] = 9; hi[5] = 9; hi[6] = 9; hi[7] = 9; hi[8] = 3;
    } //left: [0 0 0 0 0 0 0 0 4] [9 9 9 9 9 9 9 9 3]
    else if (rank == 1) { // process #1
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 4; //covers front face, 2 wide
        hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 9; hi[4] = 9; hi[5] = 9; hi[6] = 1; hi[7] = 9; hi[8] = 9;
    } //left: [2 2 2 2 2 2 2 2 4] [9 9 9 9 9 9 9 9]
    else if (rank == 2) { // process #2
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 2; lo[7] = 0; lo[8] = 4;
        hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 9; hi[4] = 9; hi[5] = 9; hi[6] = 9; hi[7] = 5; hi[8] = 9;
    }
    else if (rank == 3) { // process #3     creates a 3-wide gap in the x dimension
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 2; lo[7] = 6; lo[8] = 4;
        hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 9; hi[4] = 9; hi[5] = 4; hi[6] = 9; hi[7] = 9; hi[8] = 9;
    }
    else if (rank == 4) { // process #3     creates a 3-wide gap in the x dimension
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 5; lo[6] = 2; lo[7] = 6; lo[8] = 4;
        hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 9; hi[4] = 7; hi[5] = 9; hi[6] = 9; hi[7] = 9; hi[8] = 9;
    }
    else if (rank == 5) { // process #3     creates a 3-wide gap in the x dimension
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 8; lo[5] = 5; lo[6] = 2; lo[7] = 6; lo[8] = 4;
        hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 2; hi[4] = 9; hi[5] = 9; hi[6] = 9; hi[7] = 9; hi[8] = 9;
    }
    // else if (rank == 6) { // process #4     if not commented out, fails test (intended)
    //     lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 3; lo[4] = 8; lo[5] = 5; lo[6] = 2; lo[7] = 6; lo[8] = 4; //  2,2,0  2,2,1
    //     hi[0] = 9; hi[1] = 9; hi[2] = 9; hi[3] = 9; hi[4] = 9; hi[5] = 9; hi[6] = 9; hi[7] = 9; hi[8] = 9; //  2,2,2  2,2,4
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
    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;2
    assert(exception_was_thrown);
}