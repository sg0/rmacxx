#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    rmacxx::Window<int, LOCAL_VIEW, EXPR> win({10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    win.print("Current...");
    
    int num1, num2;

    win(1,{0}) + 2*win(1,{0}) + 3*win(1,{2}) >> win(0,{5});
    win(1,{0}) + win(1,{2}) >> win(0,{6});

    win.flush();

    // check results
    win(0,{5}) >> num1;
    win(0,{6}) >> num2;
    
    win.flush();
  
    if (win.rank() == 0)
    {
        std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{2}) = " << num1 << std::endl;
        std::cout << "win(1,{0}) + win(1,{2}) = " << num2 << std::endl;
    }

    win.wfree();

    MPI_Finalize();

    return 0;
}
