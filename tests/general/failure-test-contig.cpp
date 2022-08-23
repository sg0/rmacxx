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
        std::list<int> buff(2*5);
        std::vector<int> weak(2*5);
        //-rmacxx::is_contig<std::vector<int>, buff>;
        rmacxx::is_contig auto foo = buff;
        //win(0, {1,2},{2,6}) << weak.data();
    }

    win.flush();

    win.print("After put...");

    win.wfree();

	MPI_Finalize();

	return 0;
}
