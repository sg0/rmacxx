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
    
    // create data type
    MPI_Datatype strided_t;
    // count blocks each consisting of blocklen copies of oldtype
    // separated by stride (number of elements between start of
    // each block).
    // this is essentially type_contiguous
    MPI_Type_vector(2/*count*/, 5/*blen*/, 5/*stride*/, MPI_INT, &strided_t);
    MPI_Type_commit(&strided_t);

    rmacxx::RMACXX_Local_t<int> foo(strided_t);
    
    // put
    win(1,{0},{9}) << foo(abuf);

    win.flush(1);
    
    win.print("After PUT...");

    MPI_Type_free(&strided_t);
    win.wfree();

    MPI_Finalize();

    return 0;
}
