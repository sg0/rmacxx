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
        //std::vector<int> buff(2*5);
        //std::fill(buff.begin(), buff.end(), 3);

        std::list<int> buff(2*5);
        win(1, {1,2},{2,6}) << buff;
    }

    win.flush();

    win.print("After put...");

    win.wfree();

	MPI_Finalize();

	return 0;
}
