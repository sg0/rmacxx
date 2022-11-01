#define RMACXX_USE_FUTURES
#include "rmacxx.hpp"

#include <cassert>


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
    window.fill(2);

    double num1 = 1.234;
    double num2 = 1.234;
    double num3 = 1.234;
    double num4 = 1.234;
    
    rmacxx::Window<double,LOCAL_VIEW,EXPR> never_to_be_flushed({20,20});
    never_to_be_flushed.fill(1);
    // These expressions should never be flushed...
    1*never_to_be_flushed(0,{0,0}) >> never_to_be_flushed(0,{0,0});
    2*never_to_be_flushed(0,{0,1}) >> never_to_be_flushed(0,{0,1});
    3*never_to_be_flushed(1,{1,0}) >> never_to_be_flushed(1,{1,0});
    4*never_to_be_flushed(1,{1,1}) >> never_to_be_flushed(1,{1,1});
    never_to_be_flushed.wfree();
    window.print("Before put..");
   
    5*window(0,{0,0}) >> window(1,{0,0});
    6*window(0,{0,1}) >> window(1,{0,1});
    7*window(0,{1,0}) >> window(1,{1,0});
    8*window(0,{1,1}) >> window(1,{1,1});

    // ...but the window which does actually flush should be able to flush without problems,
    // even though the other one never finished it's expressions
    window.flush();

    window.print("After put..");

    window(1,{0,0}) >> num1;
    window(1,{0,1}) >> num2;
    window(1,{1,0}) >> num3;
    window(1,{1,1}) >> num4;

    window.flush();
    
    if (window.rank() == 0)
    {
        std::cout << num1 << " "<< num2 << std::endl;
        std::cout << num3 << " "<< num4 << std::endl;
        assert(num1==10);
        assert(num2==12);
        assert(num3==14);
        assert(num4==16);
        std::cout << "Validation PASSED." << std::endl;
    }
    
    window.wfree();

    MPI_Finalize();

    return 0;
}
