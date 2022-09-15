#include "rmacxx.hpp"
#include <math.h>

#define ENDS 0

int main(int argc, char *argv[])
{
    // USER CONFIGURABLE VALUES
    int dimensions = 9; // number of dimensions
    std::vector<int> local_buffer_dims = {3,3,3,3,3,3,3,3,3}; // size of each dimension in the local_buffer     
    //std::initializer_list<int> transfer_dims = {2,2,2,2,2,2,2,2,2}; // size of each dimension in the transfer                 (dimX,dimY,dimZ)
    std::initializer_list<int> extract_starts = {1,1,1,1,1,1,1,1,1}; // ofset when we take out from local_buffer              (i,j,k)
    std::initializer_list<int> extract_ends = {2,2,2,2,2,2,2,2,2};
    std::initializer_list<int> place_starts = {1,1,1,1,1,1,1,1,1}; // offset when we put into window
    std::initializer_list<int> place_ends = {2,2,2,2,2,2,2,2,2};

    //convert initializer lists into vectors
    std::vector<int> extract_starts_vec(dimensions);
    extract_starts_vec.insert(extract_starts_vec.end(), extract_starts.begin(), extract_starts.end());
    std::vector<int> extract_ends_vec(dimensions);
    extract_ends_vec.insert(extract_ends_vec.end(), extract_ends.begin(), extract_ends.end());
    std::vector<int> transfer_dims_vec(dimensions);
    for (int i = 0; i < dimensions; i++) {
        transfer_dims_vec[i] = extract_ends_vec[i] - extract_starts_vec[i] + 1;
    }
    //transfer_dims_vec.insert(transfer_dims_vec.end(), transfer_dims.begin(), transfer_dims.end());
    //std::vector<int> place_starts_vec(dimensions);
    //place_starts_vec.insert(place_starts_vec.end(), place_starts.begin(), place_starts.end());
   
    // create processes
    int rank;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );


    // USER CONFIGURABLE VALUES
    std::vector<int> lo(dimensions), hi(dimensions); // size of each process, inclusive coordinates
    if (rank == 0) // process #0
    { 
        lo[0] = 0; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 1; hi[1] = 1; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }
    else if (rank == 1) // process #1
    {
        lo[0] = 2; lo[1] = 0; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 3; hi[1] = 1; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }
    else if (rank == 2) // process #2
    {
        lo[0] = 0; lo[1] = 2; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 1; hi[1] = 3; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }
    else // process #3
    {
        lo[0] = 2; lo[1] = 2; lo[2] = 0; lo[3] = 0; lo[4] = 0; lo[5] = 0; lo[6] = 0; lo[7] = 0; lo[8] = 0;
        hi[0] = 3; hi[1] = 3; hi[2] = 3; hi[3] = 3; hi[4] = 3; hi[5] = 3; hi[6] = 3; hi[7] = 3; hi[8] = 3;
    }   

    // create Window
    rmacxx::Window<int,GLOBAL_VIEW> win(lo, hi);

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

    // generate the sizes of the local_buffer and the transfer
    int local_size = 1, transfer_size = 1;
    for (int i = 0; i < local_buffer_dims.size(); i++) {
        local_size *= local_buffer_dims[i];
    }
    for (int i = 0; i < transfer_dims_vec.size(); i++) {
        transfer_size *= transfer_dims_vec[i];
    }

    // fill the local_buffer with 0s
    std::vector<int> local_buffer(local_size);
    for(int i = 0; i < local_size; i++) {
        local_buffer[i] = 0;
    }
    
    // fill the part of the local_buffer that will be extracted with 3s
    int formula_result = 0; // generate first index of the transfer within the local_buffer 
    int dimensions_product;
    for (int i = 0; i < dimensions; i++) { // run once per dimension
        dimensions_product = 1;
        for (int j = i + 1; j < dimensions; j++) { // for each dimension excluding this one and all before
            dimensions_product *= transfer_dims_vec[j];
        }
        formula_result += extract_starts_vec[i] * dimensions_product;
    }

    for (int i = 0; i < transfer_size; i++) {
        local_buffer[i + formula_result] = 3;
    }

    // create subarray type for global transfer    //starts             //ends
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub(extract_starts,extract_ends);

    // put
    std::cout<<"waddup"<<std::endl;

    win(place_starts,place_ends) << sub(local_buffer.data());

    std::cout<<"reached here"<<std::endl;

    int nums[transfer_size];
    win(place_starts, place_ends) >> nums;

    std::cout<<"reached here2 electric boogaloo"<<std::endl;



/*
    win(place_starts,{
            place_starts_vec[0] + transfer_dims_vec[0] - 1,
            place_starts_vec[1] + transfer_dims_vec[1] - 1,
            place_starts_vec[2] + transfer_dims_vec[2] - 1
        }) << sub(local_buffer.data());

    std::cout<<"reached here"<<std::endl;

    int nums[transfer_size];
    win(place_starts,{
            place_starts_vec[0] + transfer_dims_vec[0] - 1,
            place_starts_vec[1] + transfer_dims_vec[1] - 1,
            place_starts_vec[2] + transfer_dims_vec[2] - 1
        }) >> nums;
*/
    
    win.flush();
    
    win.print("After put...");

    win.barrier();
    
    win.wfree();
    

    local_buffer.clear();

    MPI_Finalize();

    // run a quick assertion test
    if (rank == 0) {
        bool all_threes = true;
        for (int i = 0; i < transfer_size; i++) {
            all_threes = all_threes && nums[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

    return 0;
}
