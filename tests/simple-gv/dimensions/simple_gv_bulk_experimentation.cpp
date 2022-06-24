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
        hi[0] = 3; hi[1] = 3; hi[2] = 7;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 4; lo[1] = 0; lo[2] = 0;
        hi[0] = 7; hi[1] = 3; hi[2] = 7;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 4; lo[2] = 0;
        hi[0] = 3; hi[1] = 7; hi[2] = 7;
    }
    else // process #3
    {
        lo[0] = 4; lo[1] = 4; lo[2] = 0;
        hi[0] = 7; hi[1] = 7; hi[2] = 7;
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
    

    int in_vol = 3 * 6 * 7;

    
    //inner is 3x3x3
    for (int i = 0; i < in_vol; i++) {
       data[i + 0] = 3; //WHAT
             
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

        // INPUTTING STARTING AT 5,5,5
        //2*2*2 -> i + 35   dim z = 2; dim y = 2; dim x = 2; i*dimX*dimY + j*dimY + k
        //2*3*2 -> i + 45
        //2*3*3 -> i + 65
        
        //3*2*2 -> i + 35
        //3*3*2 -> i + 45
        //3*3*3 -> i + 65
        //3*3*4 -> i + 85
        //3*4*3 -> i + 80
        //3*4*4 -> i + 105


        //x*2*2 -> 35   5+5+5 + 2*5 + 2*5 = 35
        //x*3*2 -> 45   5+5+5 + 3*5 + 2*5
        //x*2*3 -> 50   5+5+5 + 
        //x*4*2 -> 55   5+5+5 + 4*5 + 2*5 = 
        //x*3*3 -> 65
        //x*2*4 -> 65
        //x*4*3 -> 80
        //x*3*4 -> 85
        //x*4*4 -> 105

        // INPUTTING STARTING AT 4,5,6
        //x*2*2 -> 32
        //x*3*2 -> 40
        //x*2*3 -> 45

        // INPUTTING STARTING AT 4,5,1
        //x*2*3 -> 40

        // INPUTTING STARTING AT 4,5,2
        //x*2*3 -> 41   2+2*4*2 = 18 + c + _

        //(x,y,z) (a,b) -> val
        //(0,1,0) (1,5) -> 5
        //(0,1,0) (1,6) -> 6
        //(0,1,0) (1,7) -> 7

        //(0,2,0) (1,7) -> 14
        //(0,2,1) (1,7) -> 15
        //(0,2,2) (1,7) -> 16
        //(0,2,2) (3,7) -> 16

        //(1,2,2) (1,7) -> 23   bx + az + by
        //(2,2,2) (1,7) -> 30   bx + az + by
        //(2,2,2) (6,7) -> 100
        //(1,2,2) (6,7) -> 58
        //(0,2,2) (6,7) -> 16   abx + z + by 

        // STARTS AT x,y,z and of size _,a,b and unknown constant c
        // c + z + by + abx       + _2xa_

        // i*dimX*dimY + j*dimY + k
        // x*a*b + y*b + z
        
        // x = i
        // y = j
        // z = k
        // a = dimX
        // b = dimY


        // THE FORMULA
        //(i,j,k) is the starting position
        //(dimX,dimY,dimZ) is the size of the inner volume
        //// i*dimY*dimZ + j*dimZ + k = offset from start of buffer, and read for length of dimX*dimY*dimZ
        //        ->  (3D) i*dimY*dimZ + j*dimZ + k; (4D) i*dimY*dimZ*dimA + j*dimZ*dimA + k*dimA + l



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
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({0,0,0},{3,6,7}); //extract a 2-wide hypercube
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
    win({0,0,1},{2,5,7}) << sub(data.data()); //put the 2-wide hypercube subarray into the center of the 4-wide hypercube

    std::cout<<"reached here"<<std::endl;

    int nums[in_vol];
    win({0,0,1},{2,5,7}) >> nums;
    
    //come up with a way to check sizes and bounds, and throw an error if numbers don't match up, CONCEPTS
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();
    

    data.clear();

    MPI_Finalize();

    std::cout<<temp<<std::endl;

    //std::string temporary = "";

    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < in_vol; i++) {
            all_threes = all_threes && nums[i] == 3;
            std::cout<<nums[i]<<std::endl;
        }
        assert(all_threes);
        //std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
