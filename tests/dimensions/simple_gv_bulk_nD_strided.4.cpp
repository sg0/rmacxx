#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(2), hi(2);

    // first dimension is height, then width

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0;
        hi[0] = 2; hi[1] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 0; lo[1] = 4;
        hi[0] = 4; hi[1] = 11;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 3; lo[1] = 0;
        hi[0] = 7; hi[1] = 3;
    }
    else // process #3
    {
        lo[0] = 5; lo[1] = 4;
        hi[0] = 7; hi[1] = 11;
    }    

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi); //window is 8x12 = 96

    if (win.size() != 4)
    {
        std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");

    // local buffer
    std::vector<int> data(9*8); //72, 9 tall and 8 wide     THISE IS WHERE WE TAKE THE DATA FROM
    for(int i = 0; i < 72; i++)
        data[i] = 0;

    // mark subarray parts, creates a 3x5 of 3's in the middle of the window
    for(int i = 0; i < 9; i++)
    {
        if ((i >= 2) && (i <= 6))
        {
            for(int k = 0; k < 3; k++)
                data[i*8+k+2] = 3;
        }
    }

    // print local buffer, print the window up to this point
    if (rank == 0)
    {
        std::cout << "Local buffer: " << std::endl;
        for(int i = 0; i < 9; i++) //x?
        {
            for(int k = 0; k < 8; k++) //y?
            {
                std::cout << data[i*8+k] << " ";
            }
            std::cout << std::endl;
        }
    }

    // create subarray type for global transfer
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({2,2},{9,8}); //7x6 = 42     DEFINE SUBARRAY OF DATA TO TAKE DATA FROM
    //rmacxx::RMACXX_Subarray_t<int, GLOBAL_VIEW> sub({2,2}, {4,6});

    //window is inclusive
    //subarray is exclusive

    std::vector<int> data1(3*5);
    for (int i = 0; i < 15; i++) {
        data1[i] = 3;
    }

    // put
    win({2,3},{6,5}) << sub(data.data()); //5x3 = 15
    //win({2,2},{4,6}) << sub(data1.data());
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();
    

    data.clear();

    MPI_Finalize();

    return 0;
}
