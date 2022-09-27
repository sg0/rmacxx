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
    std::vector<int> lo(3), hi(3); // size of each process, inclusive coordinates
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0; lo[2] = 0;
        hi[0] = 9; hi[1] = 9; hi[2] = 9;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 10; lo[1] = 0; lo[2] = 0;
        hi[0] = 19; hi[1] = 9; hi[2] = 9;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 10; lo[2] = 0;
        hi[0] = 9; hi[1] = 19; hi[2] = 9;
    }
    else // process #3
    {
        lo[0] = 10; lo[1] = 10; lo[2] = 0;
        hi[0] = 19; hi[1] = 19; hi[2] = 9;
    } 
    rmacxx::Window<int, GLOBAL_VIEW> win(lo, hi);
    
    if (win.size() != 4)
    {
        std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }


    win.fill(1);

    if (win.rank() == 0) {
        std::vector<int> buff(4*4*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({6,6,1},{9,9,8}) << buff.data();
    } else if (win.rank() == 1) {
        std::vector<int> buff(6*4*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({10,6,1},{15,9,8}) << buff.data();
    } else if (win.rank() == 2) {
        std::vector<int> buff(4*5*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({6,10,1},{9,14,8}) << buff.data();
    } else if (win.rank() == 3) {
        std::vector<int> buff(6*5*8);
        std::fill(buff.begin(), buff.end(), 3);
        win({10,10,1},{15,14,8}) << buff.data();
    }

    win.flush();

    double t1 = MPI_Wtime();

    int nums[720];
    win({6,6,1},{15,14,8}) >> nums;
    
    win.print("After put...");
    win.wfree();

    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

	MPI_Finalize();

    // run a quick assertion test
    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 720; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

	return 0;
}
