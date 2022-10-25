#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    rmacxx::Window<int, LOCAL_VIEW, EXPR, LOCAL_FLUSH> win({10});
    // rmacxx::Window<int, LOCAL_VIEW, EXPR> win({10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    win.print("Current...");
    
    int num1, num2, num3, num4, num5;

    win(1,{0}) + 2*win(1,{0}) + 3*win(1,{2}) >> win(0,{5});
    win(1,{0}) + win(1,{2}) >> win(0,{6});
    win(1,{1}) + 3*win(1,{4}) >> win(1,{7});
    2*win(1,{2}) + 3*win(1,{3}) >> win(1,{8});
    win(1,{2}) + 2*win(1,{0}) + 5*win(1,{3}) >> win(1,{9});

    std::cout << "About to flush" << std::endl;
    win.flush(); // NOTE: Local flush still requires a flush to preform computuation
    MPI_Barrier(MPI_COMM_WORLD);

    // check results
    win(0,{5}) >> num1;
    win(0,{6}) >> num2;
    win(1,{7}) >> num3;
    win(1,{8}) >> num4;
    win(1,{9}) >> num5;

    win.flush(); // TODO: I think this should work without this?
    MPI_Barrier(MPI_COMM_WORLD);

    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "------------------- " << std::endl;

        std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{2}) = " << num1 << std::endl;
        std::cout << "win(1,{0}) + win(1,{2}) = " << num2 << std::endl;
        std::cout << "win(1,{1}) + 3*win(1,{4}) = " << num3 << std::endl;
        std::cout << "2*win(1,{2}) + 3*win(1,{3}) = " << num4 << std::endl;
        std::cout << "win(1,{2}) + 2*win(1,{0}) + 5*win(1,{3}) = " << num5 << std::endl;
    }

    if (win.rank() == 0){
        assert(num1 == 6);
        assert(num2 == 2);
        assert(num3 == 4);
        assert(num4 == 5);
        assert(num5 == 8);
        std::cout << "Validation PASSED." << std::endl;
    }

    win.wfree();

    MPI_Finalize();

    return 0;
}
