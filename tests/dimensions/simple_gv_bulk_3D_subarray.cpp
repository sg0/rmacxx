#include "rmacxx.hpp"
#include <math.h>

//THIS TEST MUST NOT USE THE RMACXX_SUBARRAY_USE_END_COORDINATES FLAG

int main(int argc, char *argv[])
{
    // USER CONFIGURABLE VALUES
    int dimensions = 3; // number of dimensions
    std::vector<int> local_buffer_dims = {3,3,3}; // size of each dimension in the local_buffer     
    std::vector<int> transfer_dims = {2,2,2}; // size of each dimension in the transfer                 (dimX,dimY,dimZ)
    std::vector<int> extract_offset = {1,1,1}; // ofset when we take out from local_buffer              (i,j,k)
    std::vector<int> place_offset = {1,1,1}; // offset when we put into window

   
    // create processes
    int rank;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    double t0 = MPI_Wtime();


    // USER CONFIGURABLE VALUES
    std::vector<int> lo(dimensions), hi(dimensions); // size of each process, inclusive coordinates
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
    for (int i = 0; i < transfer_dims.size(); i++) {
        transfer_size *= transfer_dims[i];
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
            dimensions_product *= transfer_dims[j];
        }
        formula_result += extract_offset[i] * dimensions_product;
    }

    for (int i = 0; i < transfer_size; i++) {
        local_buffer[i + formula_result] = 3;
    }

    // create subarray type for global transfer    //starts             //sizes
    rmacxx::RMACXX_Subarray_t<int,GLOBAL_VIEW> sub({extract_offset[0], extract_offset[1], extract_offset[2]},{transfer_dims[0],transfer_dims[1],transfer_dims[2]});

    // put
    std::cout<<"waddup"<<std::endl;

    win({place_offset[0], place_offset[1], place_offset[2]},{
            place_offset[0] + transfer_dims[0] - 1,
            place_offset[1] + transfer_dims[1] - 1,
            place_offset[2] + transfer_dims[2] - 1
        }) << sub(local_buffer.data());

    std::cout<<"reached here"<<std::endl;

    int nums[transfer_size];
    win({place_offset[0], place_offset[1], place_offset[2]},{
            place_offset[0] + transfer_dims[0] - 1,
            place_offset[1] + transfer_dims[1] - 1,
            place_offset[2] + transfer_dims[2] - 1
        }) >> nums;
    
    win.flush();
    win.print("After put...");
    win.barrier();
    win.wfree();

    local_buffer.clear();

    double t1 = MPI_Wtime();
    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

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
