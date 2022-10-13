#include "rmacxx.hpp"

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    std::vector<int> lo(1), hi(1);

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0;
        hi[0] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 4;
        hi[0] = 11;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 12;
        hi[0] = 14;
    }
    else // process #3
    {
        lo[0] = 15;
        hi[0] = 17;
    }    

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

    if (win.size() != 4)
    {
        std::cout << "Number of processes should be exactly 4. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // print data ranges per process
    win.print_ranges();

    // fill window buffer
    win.fill(1); // [ 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 ]

    // print shape
    win.print("Current...");
    
    // put
    std::vector<int> data(20); // [ 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 ] 
    for(int i = 0; i < 20; i++)
        data[i] = 3;

    rmacxx::RMACXX_Subarray_t<int, GLOBAL_VIEW> rmx_vt({1}, {4});    // [ 3 {3 3 3 3} 3 3 3 3 3 3 3 3 3 3 3 3 3 3 3 ] 

    // [ 1 1 1 {1 1 1 1 1 1 1 1 1 1 1} 1 1 1 1 ]
    win({3},{6}) << rmx_vt(data.data());     

    // NOTE: For window subarrays that are too large for the provided data
    // we find an odd occurence where garbage data is used to fill some number of the available spaces.
    // This should, ideally, be checked at runtime before the put actually happens, to discourage this
    // error from occuring.
    // EX: win({3},{1}) << rmx_vt(data.data()); 
    // Oberved:            [ 1 1 1 {{3 3 3 3} 3 3 3 3 3 0 0} 1 1 1 1 ]
    // Expected(if any):   [ 1 1 1 {{3 3 3 3} 3 3 3 3 3 3 3} 1 1 1 1 ] 
    // Likely desired:     [ 1 1 1 {{3 3 3 3}} 1 1 1 1 1 1 1 1 1 1 1 ] 
    
    win.flush();

    int nums[4];
    win({3},{6}) >> nums;
    
    win.print("After put...");

    win.barrier();
   
    win.wfree();

    MPI_Finalize();

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 4; i++) {
            std::cout << nums[i] << std::endl;
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
