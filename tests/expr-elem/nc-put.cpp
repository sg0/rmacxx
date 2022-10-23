#include <cassert>
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
    std::vector<int> local_buffer(10); 
    int *abuf = local_buffer.data();
    
    // create subarray type
    rmacxx::RMACXX_Subarray_t<int> foo({1},{2},{10});

    
    // put
    win(1,{0},{9}) << foo(abuf);
    win.flush(1);

    std::vector<int> results(10);
    win(1,{0},{9}) >> results.data();
    win.flush(1);


    win.print("After: ");
    if(win.rank()==0){
        int failedIndex = -1;

        for (int i = 0; i < results.size();i++){
            if (1 <= i && i <= 2)
            {
                if (results[i] != 0) // If it was supposed to be overwritten but wasnt
                {
                    failedIndex = i;
                    break;
                }
            }else {
                if (results[i] != 1) // If it wasnt supposed to be overwritten but was
                {
                    failedIndex = i;
                    break;
                }
            }
        }

        std::cout<< "Failed index: "<<failedIndex<<std::endl;

        assert(failedIndex == -1);
    }
    
    win.wfree();

    MPI_Finalize();

    return 0;
}
