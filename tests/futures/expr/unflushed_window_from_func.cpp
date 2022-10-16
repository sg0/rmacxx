#include "rmacxx.hpp"

#include <cassert>

void create_forgotten_win(){
    rmacxx::Window<double,LOCAL_VIEW,EXPR> window({20,20});
    window.fill(1);
    1*window(0,{0,0}) >> window(0,{0,0});
    2*window(0,{0,1})  >> window(0,{0,1});
    3*window(1,{1,0}) >> window(1,{1,0});
    4*window(1,{1,1})  >> window(1,{1,1});
    window.wfree();
    // The expression objects should be forgotten here
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
    window.fill(2);

    double num1;
    double num2;
    double num3;
    double num4;

    create_forgotten_win();
    window.print("Before put..");

    // TODO: It seems that reading then writing to a location causes a race condition.
    // Is there a way to resolve this race condition by running only the target process?
    // Maybe an mpi barrier? if &only if reads and writes contain any of the same elems
    // Would that introduce too much overhead? O(n) cycles where n is number of dimensions
    // 5*window(0,{0,0}) >> window(0,{0,0});
    // 6*window(0,{0,1}) >> window(0,{0,1});
    // 7*window(1,{1,0}) >> window(1,{1,0});
    // 8*window(1,{1,1}) >> window(1,{1,1});
    5*window(0,{0,0}) >> window(1,{0,0});
    6*window(0,{0,1}) >> window(1,{0,1});
    7*window(0,{1,0}) >> window(1,{1,0});
    8*window(0,{1,1}) >> window(1,{1,1});

    // The window which does actually exist should be able to flush without problems,
    // even though the other one never finished it's expressions, and was even dropped
    window.flush();
    window.print("After put..");

    window(1,{0,0}) >> num1;
    window(1,{0,1}) >> num2;
    window(1,{1,0}) >> num3;
    window(1,{1,1}) >> num4;

    window.flush();
    
    if (window.rank() == 0)
    {
        std::cout << num1 << " "<<num2 << std::endl;
        std::cout << num3 << " "<<num4 << std::endl;
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
