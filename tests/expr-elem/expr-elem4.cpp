#include "rmacxx.hpp"

#include <cassert>

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    int num = -1;
    
    // create window for expression
    rmacxx::Window<int, LOCAL_VIEW, EXPR> win({10});
    
    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);
    win.print("Current...");

    // expressions...
    //win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) >> num;
    //3*win(1,{0}) >> num;
    win(1,{0}) + win(0,{1}) >> num;
   
    // in this case, a flush is needed to
    // complete the expression evaluations
    win.flush();
    
    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "------------------- " << std::endl;

        std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) = " << num << std::endl;
    }

    win.wfree();

    MPI_Finalize();

    return 0;
}
