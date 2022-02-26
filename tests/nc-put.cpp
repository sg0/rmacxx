// simple test to demonstrate buffer
// update in non-contiguous locations

#include "rmacxx.hpp"

int main(int argc, char *argv[])
{    
    MPI_Init( &argc, &argv );
   
    // create window
    rmacxx::Window<int> win({10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);
    win.print("Initially...");

    // create a vector, initially 0s
    std::vector<int> a(10); 
    int *abuf = a.data();
    
    // create subarray type
    rmacxx::RMACXX_Subarray_t<int> foo({1},{2},{10});
    
    // put
    win(1,{0},{9}) << foo(abuf);

    win.flush(1);
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
