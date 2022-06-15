#include "rmacxx.hpp"
#include <math.h>

//#define ENDS 0

int main(int argc, char *argv[])
{
    int rank;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    
    int dimensions = 3; // number of dimensions

    std::vector<int> lo(dimensions), hi(dimensions);

    // create window
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0; lo[2] = 0;
        hi[0] = 1; hi[1] = 1; hi[2] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 2; lo[1] = 0; lo[2] = 0;
        hi[0] = 3; hi[1] = 1; hi[2] = 3;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 2; lo[2] = 0;
        hi[0] = 1; hi[1] = 3; hi[2] = 3;
    }
    else // process #3
    {
        lo[0] = 2; lo[1] = 2; lo[2] = 0;
        hi[0] = 3; hi[1] = 3; hi[2] = 3;
    }   

    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi); //window is a 4-wide hypercube

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
    int size = 3 * 3 * 3; // 3-wide 3-dimensional cube
    std::vector<int> data(size); //3^3
    for(int i = 0; i < 27; i++) {
        data[i] = 0;
    }
    
    /*
    for(int a = 0; a < 2; a++)
    {
        for(int b = 0; b < 2; b++)
        {
            for(int c = 0; c < 2; c++)
            {
                data[(c + b*3 + a*9)] = 3; //range of 0 to 19682 = 3^9 - 1
            }
        }
    }

    //TEMP
    for (int i = 0; i < 27; i++) {
        data[i] = i + 100;
    }
    */

    for (int i = 0; i < 8; i++) {
        data[i + 1 + 2 + 4] = 3;
    }

    int temp = data[data.size() - 1];

    // create subarray type for global transfer    //starts //sizes
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({1,1,1},{2,2,2}); //extract a 2-wide hypercube
    //  i,j,k

    // DIMS REFERS TO THE AREA EXTRACTED BY THE SUBARRAY, or something like that
    // i*dims[0]*dims[1] + j*dims[1] + k
    // NOTE dims of the Subarray sizes

    //setting 1st start to 1 skips 4    2^2
    //setting 2nd start to 1 skips 2    2^1
    //setting 3rd start to 1 skips 1    2^0

    // xx xx 
    // xx xx 

    /*
    subarray multiplies the size dimensions and takes that long of a *contiguous* chunk of data
    the start dimensions are 
    */




    //std::vector<int> temp_vec(8);

    //temp_vec << sub(data.data()); //better
    //sub(data.data()) >> temp_vec;

    //don't create an intermediate object, and make sure the c++ interface we use isn't doing that either




    //sub(data.data()) >> temp_vec;
    //rmacxx::RMACXX_Subarray_t<int, GLOBAL_VIEW> sub({2,2}, {4,6});

    //window is inclusive coordinates
    //subarray is start coordinate and size

    // put
    
    win({1,1,1},{2,2,2}) << sub(data.data()); //put the 2-wide hypercube subarray into the center of the 4-wide hypercube

    std::cout<<"reached here"<<std::endl;

    int nums[8];
    win({1,1,1},{2,2,2}) >> nums;
    
    //come up with a way to check sizes and bounds, and throw an error if numbers don't match up, CONCEPTS
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();
    

    data.clear();

    MPI_Finalize();

    std::cout<<temp<<std::endl;

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < 8; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
