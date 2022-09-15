#include "rmacxx.hpp"
#include <list>

#include <cassert>

int main(int argc, char *argv[])
{

	MPI_Init(&argc, &argv);
    double t0 = MPI_Wtime();

	int num1, num2, num3, num4, num5;

    rmacxx::Window<int, LOCAL_VIEW> win({10,10});
    win.fill(1);

    std::vector<int> buff(2*5);
    std::fill(buff.begin(), buff.end(), 3);
    win(win.rank(), {1,2},{2,6}) << buff.data();

/*
    if (win.rank() == 0) {
        std::vector<int> buff(2*5);
        std::fill(buff.begin(), buff.end(), 3);
        win(0, {1,2},{2,6}) << buff.data();
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
    } else if (win.rank() == 4) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(4, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 5) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(5, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 6) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(6, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 7) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(7, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 8) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(8, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 9) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(9, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 10) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(10, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 11) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(11, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 12) {
        std::vector<int> buff(2*5);
        std::fill(buff.begin(), buff.end(), 3);
        win(12, {1,2},{2,6}) << buff.data();
    } else if (win.rank() == 13) {
        std::vector<int> buff(2*1);
        std::fill(buff.begin(), buff.end(), 3);
        win(13, {4,2},{5,2}) << buff.data();
    } else if (win.rank() == 14) {
        std::vector<int> buff(3*1);
        std::fill(buff.begin(), buff.end(), 3);
        win(14, {2,6},{4,6}) << buff.data();
    } else if (win.rank() == 15) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(15, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 16) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(16, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 17) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(17, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 18) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(18, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 19) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(19, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 20) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(20, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 21) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(21, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 22) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(22, {0,2},{3,3}) << buff.data();
    } else if (win.rank() == 23) {
        std::vector<int> buff(4*2);
        std::fill(buff.begin(), buff.end(), 3);
        win(23, {0,2},{3,3}) << buff.data();
    }
*/

    win.flush();

    win.print("After put...");

    win.wfree();

    double t1 = MPI_Wtime();
    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

	MPI_Finalize();

	return 0;
}
