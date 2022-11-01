#define RMACXX_USE_FUTURES
#include "rmacxx.hpp"

#include <cassert>

void scale_win(rmacxx::Window<double,LOCAL_VIEW,EXPR>& window){
    rmacxx::Window<double,LOCAL_VIEW,EXPR> other({20,20});
    other.fill(1);
    1*(other(0,{0,0}) + window(0,{0,0})) >> window(1,{0,0});
    2*(other(0,{0,1}) + window(0,{0,1})) >> window(1,{0,1});
    3*(other(0,{1,0}) + window(0,{1,0})) >> window(1,{1,0});
    4*(other(0,{1,1}) + window(0,{1,1})) >> window(1,{1,1});
    other.flush();
    window.flush();

    other.wfree();
}
    
int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    rmacxx::Window<double,LOCAL_VIEW,EXPR> window({20,20});

    if (window.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }


    // fill window buffer
    window.fill(1);

    double num1;
    double num2;
    double num3;
    double num4;

    scale_win(window);
    window.flush();

    window(1,{0,0}) >> num1;
    window(1,{0,1}) >> num2;
    window(1,{1,0}) >> num3;
    window(1,{1,1}) >> num4;

    window.flush();
    
    if (window.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "--------------------" << std::endl;
        std::cout << num1 << " "<<num2 << std::endl;
        std::cout << num3 << " "<<num4 << std::endl;
        assert(num1==2);
        assert(num2==4);
        assert(num3==6);
        assert(num4==8);
        std::cout << "Validation PASSED." << std::endl;
    }
    
    window.wfree();

    MPI_Finalize();

    return 0;
}
