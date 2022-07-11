#include "rmacxx.hpp"
#include <list>

#include <cassert>

int main(int argc, char *argv[])
{

	int rank;
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    double t0 = MPI_Wtime();

	int num1, num2, num3, num4, num5;

    rmacxx::Window<int, LOCAL_VIEW> win({10,10,10});
    win.fill(1);

    if (win.rank() == 0) {
        std::vector<int> buff(4*4*8);
        std::fill(buff.begin(), buff.end(), 3);
        win(0, {6,6,1},{9,9,8}) << buff.data();
    } else if (win.rank() == 1) {
        std::vector<int> buff(6*4*8);
        std::fill(buff.begin(), buff.end(), 3);
        win(1, {0,6,1},{5,9,8}) << buff.data();
    } else if (win.rank() == 2) {
        std::vector<int> buff(4*5*8);
        std::fill(buff.begin(), buff.end(), 3);
        win(2, {6,0,1},{9,4,8}) << buff.data();
    } else if (win.rank() == 3) {
        std::vector<int> buff(6*5*8);
        std::fill(buff.begin(), buff.end(), 3);
        win(3, {0,0,1},{5,4,8}) << buff.data();
    }

    win.flush();

    double t1 = MPI_Wtime();

    win.print("After put...");

    win.wfree();

    
    std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

	MPI_Finalize();

	return 0;
}
