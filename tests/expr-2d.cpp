#include "rmacxx-expr-elem.hpp"

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    rmacxx::Lwindow<int> win({5,5});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    win.barrier();
    win.print("Current...");
    
    int num;
    rmacxx::EwiseExpr<int> w(win);
    w(1,{0,0}) + 2*w(1,{0,0}) + 3*w(1,{0,0}) >> num;

    // no flush needed as expression is locally complete
    
    std::cout << "Expression results: " << num << std::endl;

    win.wfree();

    win.barrier();
    MPI_Finalize();

    return 0;
}
