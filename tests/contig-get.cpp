// simple test to demonstrate buffer
// update in non-contiguous locations

#include "rmacxx.hpp"

int main(int argc, char *argv[])
{    
    MPI_Init( &argc, &argv );
   
    // create window
    rmacxx::Window<int> win({30});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    // create a vector and fill
    std::vector<int> a(10); 
    int *abuf = a.data();
    
    // create data type
    MPI_Datatype contig_t;
    // count blocks each consisting of blocklen copies of oldtype.
    MPI_Type_contiguous(5 /*count*/, MPI_INT, &contig_t);
    MPI_Type_commit(&contig_t);
 
    // get
    win(1,{10},{19}) >> rmacxx::MPI_type<int>(abuf, contig_t, 2);

    win.flush(1);
    win.print("Window contents...");

    for (int r = 0; r < win.size(); r++)
    {
        if (r == win.rank())
        {
            std::cout << std::endl;
            std::cout << "Rank: " << r << std::endl;
            std::cout << "--------------" << std::endl;
            for (int i = 0; i < 10; i++)
                std::cout << abuf[i] << " ";
            std::cout << std::endl;
        }
        win.barrier();
    }

    MPI_Type_free(&contig_t);
    win.wfree();

    MPI_Finalize();

    return 0;
}
