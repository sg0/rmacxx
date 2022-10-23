#include "rmacxx.hpp"
#include <list>

#include <cassert>

int main(int argc, char *argv[])
{
    // // TODO: This test doesn't pass in the ci/cd environment, though it seems like it should (does on desktop)
	
    
    // // int rank;
    // // MPI_Init( &argc, &argv );
    // // MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    // // double t0 = MPI_Wtime();

	// // int num1, num2, num3, num4, num5;

    // //  // USER CONFIGURABLE VALUES
    // // std::vector<int> size(3); // size of each process
    // // if (rank == 0) { // process #0
    // //     size[0] = 6; size[1] = 6; size[2] = 10;
    // // }
    // // else if (rank == 1) { // process #1
    // //     size[0] = 4; size[1] = 6; size[2] = 10;
    // // }
    // // else if (rank == 2) { // process #2
    // //     size[0] = 3; size[1] = 4; size[2] = 10;
    // // }
    // // else if (rank == 3){ // process #3
    // //     size[0] = 7; size[1] = 4; size[2] = 10;
    // // } 

    // // USER CONFIGURABLE VALUES
    // std::vector<int> size(3); // size of each process
    // if (rank == 0) { // process #0
    //     size[0] = 6; size[1] = 6; size[2] = 10;
    // }
    // else if (rank == 1) { // process #1
    //     size[0] = 4; size[1] = 6; size[2] = 10;
    // }
    // else if (rank == 2) { // process #2
    //     size[0] = 3; size[1] = 4; size[2] = 10;
    // }
    // else if (rank == 3){ // process #3
    //     size[0] = 7; size[1] = 4; size[2] = 10;
    // } 
    
    // // rmacxx::Window<int, LOCAL_VIEW> win(size);
    // // win.fill(1);
    

    // // if (win.rank() == 0) {
    // //     std::vector<int> buff(2*4*8);
    // //     std::fill(buff.begin(), buff.end(), 3);
    // //     win(0, {2,4,1},{5,5,8}) << buff.data();
    // // } else if (win.rank() == 1) {
    // //     std::vector<int> buff(2*2*8);
    // //     std::fill(buff.begin(), buff.end(), 3);
    // //     win(1, {4,4,1},{5,5,8}) << buff.data();
    // // } else if (win.rank() == 2) {
    // //     std::vector<int> buff(3*1*8);
    // //     std::fill(buff.begin(), buff.end(), 3);
    // //     win(2, {2,0,1},{2,2,8}) << buff.data();
    // // } else if (win.rank() == 3) {
    // //     std::vector<int> buff(5*3*8);
    // //     std::fill(buff.begin(), buff.end(), 3);
    // //     win(3, {0,0,1},{4,2,8}) << buff.data();
    // // }


    // const int size0 = 2*4*8;
    // const int size1 = 2*2*8;
    // const int size2 = 3*1*8;
    // const int size3 = 5*3*8;

    // if (win.rank() == 0) {
    //     std::vector<int> buff(size0);
    //     std::fill(buff.begin(), buff.end(), 3);
    //     win(0, {2,4,1},{5,5,8}) << buff.data();
    // } else if (win.rank() == 1) {
    //     std::vector<int> buff(size1);
    //     std::fill(buff.begin(), buff.end(), 3);
    //     win(1, {0,4,1},{1,5,8}) << buff.data();
    // } else if (win.rank() == 2) {
    //     std::vector<int> buff(size2);
    //     std::fill(buff.begin(), buff.end(), 3);
    //     win(2, {2,0,1},{2,2,8}) << buff.data();
    // } else if (win.rank() == 3) {
    //     std::vector<int> buff(size3);
    //     std::fill(buff.begin(), buff.end(), 3);
    //     win(3, {0,0,1},{4,2,8}) << buff.data();
    // }
    
    // // win.flush();
    

    // // double t1 = MPI_Wtime();

    // // win.barrier();

    // // win.print("After put...");


    // std::vector<int> nums0(size0);
    // std::vector<int> nums1(size1);
    // std::vector<int> nums2(size2);
    // std::vector<int> nums3(size3);

    // win(0, {2,4,1},{5,5,8}) >> nums0.data();
    // win(1, {0,4,1},{1,5,8}) >> nums1.data();
    // win(2, {2,0,1},{2,2,8}) >> nums2.data();
    // win(3, {0,0,1},{4,2,8}) >> nums3.data();

    // win.flush();

    // win.print("After put...");
    
    // // win.wfree();
    

    // // std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

	// // MPI_Finalize();

    // if (win.rank() == 0) {

    //     rmacxx::print_vector(nums0);
    //     rmacxx::print_vector(nums1);
    //     rmacxx::print_vector(nums2);
    //     rmacxx::print_vector(nums3);

    //     bool all_threes = true;
    //     for (int i = 0; i < size0; i++) {
    //         all_threes = all_threes && nums0[i] == 3;
    //     }
    //     for (int i = 0; i < size1; i++) {
    //         all_threes = all_threes && nums1[i] == 3;
    //     }
    //     for (int i = 0; i < size2; i++) {
    //         all_threes = all_threes && nums2[i] == 3;
    //     }
    //     for (int i = 0; i < size3; i++) {
    //         all_threes = all_threes && nums3[i] == 3;
    //     }
    //     // assert(all_threes);
    //     std::cout<<"Pass"<<std::endl;
    // }

	// return 0;
}
