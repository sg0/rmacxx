#include "rmacxx.hpp"
#include <list>

#include <cassert>
#include <unistd.h>

int main(int argc, char *argv[]) {

    std::vector<std::vector<int>> los(4), his(4);

    int rank;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    double t0 = MPI_Wtime();

    std::vector<int> lo(3), hi(3); // size of each process, inclusive coordinates
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0; lo[2] = 0;
        hi[0] = 5; hi[1] = 5; hi[2] = 9;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 6; lo[1] = 0; lo[2] = 0;
        hi[0] = 9; hi[1] = 5; hi[2] = 9;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 6; lo[2] = 0;
        hi[0] = 2; hi[1] = 9; hi[2] = 9;
    }
    else // process #3
    {
        lo[0] = 3; lo[1] = 6; lo[2] = 0;
        hi[0] = 9; hi[1] = 9; hi[2] = 9;
    }

    //all processes fill in their own coordinates
    los[rank] = lo;
    his[rank] = hi;

    rmacxx::Window<int, GLOBAL_VIEW> win(lo, hi); //this is a barrier for all the processes 

    //test the function
    if (rank == 0) {
        std::vector<int> winsize{10, 10, 10};
        std::cout<<"RESULT: "<<rmacxx::check_gaps(winsize, los, his)<<std::endl;
    }
}

