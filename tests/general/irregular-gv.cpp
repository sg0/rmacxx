#include "rmacxx.hpp"
#include <list>

#include <cassert>
#include <unistd.h>

int main(int argc, char *argv[])
{

	int rank;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    double t0 = MPI_Wtime();

    // USER CONFIGURABLE VALUES
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

    rmacxx::Window<int, GLOBAL_VIEW> win(lo, hi); //this is a barrier for all the processes
    win.fill(1);

    if (rank == 0) {
        std::vector<int> buff(4*2*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({2,4,1},{5,5,8}) << buff.data();
    } else if (rank == 1) {
        std::vector<int> buff(2*2*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({6,4,1},{7,5,8}) << buff.data();
    } else if (rank == 2) {
        std::vector<int> buff(1*3*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({2,6,1},{2,8,8}) << buff.data();
    } else if (rank == 3) {
        std::vector<int> buff(5*3*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({3,6,1},{7,8,8}) << buff.data();
    }

    win.flush();

    double t1 = MPI_Wtime();

    win.barrier();

    int nums[240];
    win({2,4,1},{7,8,8}) >> nums;
    
    win.print("After put...");
    win.wfree();

    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

	MPI_Finalize();

    // run a quick assertion test
    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 240; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

	return 0;
}
