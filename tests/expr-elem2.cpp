#include "rmacxx.hpp"

#include <cassert>

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    int num1 = -1, num2 = -1, num3 = -1, num4 = -1, num5 = -1;
    
    // create window for expression
    rmacxx::Window<int, EXPR> win({10});
    //rmacxx::Window<int, EXPR, LOCAL_FLUSH> win({10});
    
    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);
    win.print("Current...");

    // expressions...
    win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) >> num1;
    win(1,{0}) + win(1,{0}) >> num2;
    win(1,{0}) + 3*win(1,{4}) >> num3;
    2*win(1,{0}) + 3*win(1,{0}) >> num4;
    win(1,{2}) + 2*win(1,{0}) + 5*win(1,{0}) >> num5;
    
    // in this case, a flush is needed to
    // complete the expression evaluations
    win.flush();
    
    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "------------------- " << std::endl;

        std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) = " << num1 << std::endl;
        std::cout << "win(1,{0}) + win(1,{0}) = " << num2 << std::endl;
        std::cout << "win(1,{0}) + 3*win(1,{4}) = " << num3 << std::endl;
        std::cout << "2*win(1,{0}) + 3*win(1,{0}) = " << num4 << std::endl;
        std::cout << "win(1,{2}) + 2*win(1,{0}) + 5*win(1,{0}) = " << num5 << std::endl;
        std::cout << "Validation PASSED." << std::endl;
    }

    win.wfree();

    MPI_Finalize();

    return 0;
}
