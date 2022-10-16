#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    int rank, size;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    
    std::vector<int> lo(2), hi(2);

    // first dimension is height, then width

    // create window
    lo[0] = rank; lo[1] = 0;
    hi[0] = rank; hi[1] = 9;

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");
    
    // put
    std::vector<int> data(10);
    for(int i = 0; i < 10; i++)
        data[i] = 3;
    
    win({0,0},{1,4}) << data.data();
    
    win.flush();

    int nums[10];
    win({0,0},{1,4}) >> nums;
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 10; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
