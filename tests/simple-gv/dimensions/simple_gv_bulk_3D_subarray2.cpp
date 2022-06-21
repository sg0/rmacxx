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
    int size = 10 * 10 * 10; // 3-wide 3-dimensional cube
    std::vector<int> data(size); //3^3
    for(int i = 0; i < 1000; i++) {
        data[i] = 0;
    }
    
    
    //TEMP
    for (int i = 0; i < 1000; i++) {
        data[i] = i + 100;
    }
    

    int in_vol = 2 * 3 * 3;

    
    //inner is 3x3x3
    for (int i = 0; i < in_vol; i++) {
       data[i + 4] = 3; //WHAT
             
        //   _  2^0 2^1 3^2
        // i + 10
        // i + 2 2 3


        // INPUTTING STARTING AT 1,1,1
        //2*2*2 -> i + 7    1+2+2+2
        //2*2*3 -> i + 10   i + 2^0 + 2 * 1 + 3 * 2
        //2*3*3 -> i + 13
        //3*3*3 -> i + 13 

        // INPUTTING STARTING AT 0,1,1
        //2*2*2 -> i + 3
        //2*2*3 -> i + 4
        //2*3*3 -> i + 4
        //3*3*3 -> i + 4
    }
    

    /*
    //we want to extract a 2*2*2
    std::vector<int> extract_dims(dimensions), extract_offset(dimensions);
    extract_dims[0] = 2; extract_dims[1] = 2; extract_dims[2] = 2;

    for(int i = 0; i < extract_dims[0]; i++) {
        for(int j = 0; j < extract_dims[1]; j++){
            for(int k = 0; k < extract_dims[2]; k++){
                data[i*extract_dims[0]*extract_dims[1] + j*extract_dims[1] + k] = 3;
            }
        }
    }
    */



    int temp = data[data.size() - 1];

    // create subarray type for global transfer    //starts //sizes
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({0,1,1},{2,3,3}); //extract a 2-wide hypercube
    //  i,j,k

    // DIMS REFERS TO THE AREA EXTRACTED BY THE SUBARRAY, or something like that
    // i*dims[0]*dims[1] + j*dims[1] + k
    // NOTE dims of the Subarray sizes

    /*
    subarray multiplies the size dimensions and takes that long of a *contiguous* chunk of data
    the start dimensions are 
    */

    //temp_vec << sub(data.data()); //better
    //don't create an intermediate object, and make sure the c++ interface we use isn't doing that either

    std::cout<<"about to put"<<std::endl;

    // put
    win({0,1,1},{1,3,3}) << sub(data.data()); //put the 2-wide hypercube subarray into the center of the 4-wide hypercube

    std::cout<<"reached here"<<std::endl;

    int nums[in_vol];
    win({0,1,1},{1,3,3}) >> nums;
    
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
        for (int i = 0; i < in_vol; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
