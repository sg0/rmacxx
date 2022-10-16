//#include "rmacxx.hpp"
//
//#include <cassert>
//
//int main(int argc, char *argv[])
//{
//
//    MPI_Init( &argc, &argv );
//
//    int num1, num2, num3, num4, num5;
//
//    // create window for expression
//    rmacxx::Window<int, LOCAL_VIEW, EXPR,NO_FLUSH> win({1});
//    //rmacxx::Window<int, LOCAL_VIEW, EXPR, REMOTE_FLUSH> win({10});
//
//    if (win.size() != 1)
//    {
//        std::cout << "Number of processes should be exactly 1. Aborting..." << std::endl;
//        MPI_Abort(MPI_COMM_WORLD, -99);
//    }
//
//    // fill window buffer
//    win.fill(1);
//    win.print("Current...");
//
//    // expressions...
//    (win(0,{0}) + 2*win(0,{0}) + 3*win(0,{0})) >> num1;
//    (win(0,{0}) + win(0,{0})) >> num2;
//    win(0,{0}) + 3*win(0,{0}) >> num3;
//    2*win(0,{0}) + 3*win(0,{0}) >> num4;
//    win(0,{0}) + 2*win(0,{0}) + 5*win(0,{0}) >> num5;
//
//    // in this case, a flush is needed to
//    // complete the expression evaluations
//    win.flush();
//
//    // check results
//    assert(num1 == 6);
//    assert(num2 == 2);
//    assert(num3 == 4);
//    assert(num4 == 5);
//    assert(num5 == 8);
//
//    if (win.rank() == 0)
//    {
//        std::cout << "Expression results: " << std::endl;
//        std::cout << "------------------- " << std::endl;
//
//        std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) = " << num1 << std::endl;
//        std::cout << "win(1,{0}) + win(1,{0}) = " << num2 << std::endl;
//        std::cout << "win(1,{0}) + 3*win(1,{4}) = " << num3 << std::endl;
//        std::cout << "2*win(1,{0}) + 3*win(1,{0}) = " << num4 << std::endl;
//        std::cout << "win(1,{2}) + 2*win(1,{0}) + 5*win(1,{0}) = " << num5 << std::endl;
//        std::cout << "Validation PASSED." << std::endl;
//    }
//
//    MPI_Barrier(MPI_COMM_WORLD);
//
//    win.wfree();
//
//    MPI_Finalize();
//
//    return 0;
//}
int main(int argc, char *argv[]){
}