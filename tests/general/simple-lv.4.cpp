#include "rmacxx.hpp"
#include <list>

#include <cassert>

int main(int argc, char *argv[])
{

	MPI_Init(&argc, &argv);

	int num1, num2, num3, num4, num5;

    rmacxx::Window<int, LOCAL_VIEW> win({10,10});
    win.fill(1);

    if (win.rank() == 0) {
        std::vector<int> buff(2*5);
        std::fill(buff.begin(), buff.end(), 3);
        win(0, {1,2},{2,6}) << buff.data();

        //std::list<int> buff(2*5);
        //win(1, {1,2},{2,6}) << buff;
    } else if (win.rank() == 1) {
        std::vector<int> buff(2*1);
        std::fill(buff.begin(), buff.end(), 3);
        win(1, {4,2},{5,2}) << buff.data();
    } else if (win.rank() == 2) {
        std::vector<int> buff(3*1);
        std::fill(buff.begin(), buff.end(), 3);
        win(2, {2,6},{4,6}) << buff.data();
    } else if (win.rank() == 3) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(3, {0,2},{3,3}) << buff.data();
    }

    win.flush();
    MPI_Barrier(MPI_COMM_WORLD);

    int nums0[10];
    int nums1[2];
    int nums2[3];
    int nums3[8];

    win(0, {1,2},{2,6}) >> nums0;
    win(1, {4,2},{5,2}) >> nums1;
    win(2, {2,6},{4,6}) >> nums2;
    win(3, {0,2},{3,3}) >> nums3;

    win.flush();

    win.print("After put...");

    win.wfree();

	MPI_Finalize();

    // run a quick assertion test
    if (win.rank() == 0) {
        bool all_threes = true;
        for (int i = 0; i < 10; i++) {
            all_threes = all_threes && nums0[i] == 3;
        }
        for (int i = 0; i < 2; i++) {
            all_threes = all_threes && nums1[i] == 3;
        }
        for (int i = 0; i < 3; i++) {
            all_threes = all_threes && nums2[i] == 3;
        }
        for (int i = 0; i < 8; i++) {
            all_threes = all_threes && nums3[i] == 3;
        }
        assert(all_threes);
        std::cout<<"Pass"<<std::endl;
    }

	return 0;
}
