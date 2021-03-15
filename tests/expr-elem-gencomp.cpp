#include "rmacxx.hpp"

#include <cassert>

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    int num1, num2, num3, num4, num5;
    // create window for expression
    // this will result in compilation error
    // because window participates
    // in an expression here -----
    // rmacxx::Window<int> win({10});
    // this compiles though ------
    rmacxx::Window<int, GEN_COMP> win({10});
    // if another window is in RHS to >>, then
    // if LOCAL/REMOTE_FLUSH is not specified
    // along with NB_COMP, an error is thrown
    // -----
    //rmacxx::Window<int, NB_COMP, LOCAL_FLUSH> win({10});
    //rmacxx::Window<int> win1({10});

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
    
    // check results
    assert(num1 == 6);
    assert(num2 == 2);
    assert(num3 == 4);
    assert(num4 == 5);
    assert(num5 == 8);
    
    // gen comp, so we can use = too
    num1 = win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0});
    num2 = win(1,{0}) + win(1,{0});
    num3 = win(1,{0}) + 3*win(1,{4});
    num4 = 2*win(1,{0}) + 3*win(1,{0});
    num5 = win(1,{2}) + 2*win(1,{0}) + 5*win(1,{0});

    // check results
    assert(num1 == 6);
    assert(num2 == 2);
    assert(num3 == 4);
    assert(num4 == 5);
    assert(num5 == 8);

    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "------------------- " << std::endl;

        std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) = " << num1 << std::endl;
        std::cout << "win(1,{0}) + win(1,{0}) = " << num2 << std::endl;
        std::cout << "win(1,{0}) + 3*win(1,{4}) = " << num3 << std::endl;
        std::cout << "2*win(1,{0}) + 3*win(1,{0}) = " << num4 << std::endl;
        std::cout << "win(1,{2}) + 2*win(1,{0}) + 5*win(1,{0}) = " << num5 << std::endl;
    }

    win.wfree();

    MPI_Finalize();

    return 0;
}
