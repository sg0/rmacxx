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

    
    //we want to extract a 2*3*4
    std::vector<int> extract_dims(dimensions), extract_offset(dimensions);
    extract_dims[0] = 2; extract_dims[1] = 3; extract_dims[2] = 4;
    //extract_offset

    // local buffer
    int size = 5 * 9 * 6; //size of the shape we want to extract from
    std::vector<int> data(size);
    for(int i = 0; i < size; i++) {
        data[i] = 0;
    }
    
    for (int i = 0; i < 8; i++) { //volume of shape we want to extract as the for-loop counter, cube is 2x2x2
        data[i + 1 + 2 + 4] = 3; // 3x9x7
        //   _  2^0 2^1 2^2
    }

    for(int i = 0; i < extract_dims[0]; i++) {
        for(int j = 0; j < extract_dims[1]; j++){
            for(int k = 0; k < extract_dims[2]; k++){
                
            }
        }
    }

    int temp = data[data.size() - 1];

    // create subarray type for global transfer    //starts //sizes
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({1,1,1},{2,2,2}); //extract a 2-wide hypercube
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

/*
// local_buffer is the target we're trying to fill, extract_dims is the size of the shape we want to extract, 
// and dims_index should be an empty vector
std::vector<int> fill_local_buffer(std::vector<int> local_buffer, std::vector<int> extract_dims, std::vector<int> dims_index) {
    std::vector<int> recurse_dims(extract_dims.begin() + 1, extract_dims.end());
    for (int i = 0; i < extract_dims[0]; i++) {
        dims_index.insert()
        fill_local_buffer(local_buffer, recurse_dims, dims_index);
    }


}
*/

//std::vector<int> fill_local_buffer(std::vector<int> local_buffer, )

4x9x3x5


std::vector<int> fill_local_buffer(std::vector<int> local_buffer, std::vector<int> extract_dims, int addend) {

    new_addend = addend + calculated_value

}