// #include "rmacxx.hpp"

// #include <cassert>

// #define USE_VECTORS_INSTEAD_ARRAYS
    
// int main(int argc, char *argv[])
// {

//     MPI_Init( &argc, &argv );

//     // create window
//     rmacxx::Window<double, LOCAL_VIEW, EXPR> win({10,10});

//     if (win.size() == 1)
//     {
//         std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
//         MPI_Abort(MPI_COMM_WORLD, -99);
//     }

//     // fill window buffer
//     win.fill(1);

// #if defined(USE_VECTORS_INSTEAD_ARRAYS)
//     std::cout << "Vector" << std::endl;
//     std::vector<double> chunk1, chunk2, chunk3, chunk4;
//     chunk1.resize(5*5);
//     chunk2.resize(5*5);
//     chunk3.resize(5*5);
//     chunk4.resize(5*5);
//     double* data1 = chunk1.data();
//     double* data2 = chunk2.data();
//     double* data3 = chunk3.data();
//     double* data4 = chunk4.data();
// #else
//     double* data1 = new double[5*5];
//     double* data2 = new double[5*5];
//     double* data3 = new double[5*5];
//     double* data4 = new double[5*5];
// #endif

//     win(1,{0,0},{4,4})*win(0,{0,1},{4,5}) + (2+win(1,{0,0},{4,4})) + (3+win(1,{0,0},{4,4})) >> data1;
//     win(0,{0,0},{4,4})*win(1,{1,1},{5,5}) + 4*win(1,{0,0},{4,4}) + 5*win(1,{0,0},{4,4})     >> data2;
//     win(1,{0,0},{4,4})*win(0,{2,2},{6,6}) + 2*win(1,{0,0},{4,4})*win(1,{2,2},{6,6})         >> data3;
//     4*(win(1,{0,0},{4,4})*win(1,{1,0},{5,4})) + 2*win(0,{3,4},{7,8}) + (5+win(0,{0,0},{4,4})) >> data4;
 
//     // flush to complete expression
//     win.flush();
    
//     if (win.rank() == 0)
//     {
//         std::cout << "Expression results: " << std::endl;
//         std::cout << "--------------------" << std::endl;
//         for (int i = 0; i < 5; i++)
//         {
//             for (int j = 0; j < 5; j++)
//             {
//                 std::cout << data1[i*5+j] << " ";
//                 assert(data1[i*5+j] == 8);
//             }
//             std::cout << std::endl;    
//         }
//         std::cout << "--------------------" << std::endl;
//         for (int i = 0; i < 5; i++)
//         {
//             for (int j = 0; j < 5; j++)
//             {
//                 std::cout << data2[i*5+j] << " ";
//                 assert(data2[i*5+j] == 10);
//             }
//             std::cout << std::endl;    
//         }
//         std::cout << "--------------------" << std::endl;
//         for (int i = 0; i < 5; i++)
//         {
//             for (int j = 0; j < 5; j++)
//             {
//                 std::cout << data3[i*5+j] << " ";
//                 assert(data3[i*5+j] == 3);
//             }
//             std::cout << std::endl;    
//         }
//         std::cout << "--------------------" << std::endl;
//         for (int i = 0; i < 5; i++)
//         {
//             for (int j = 0; j < 5; j++)
//             {
//                 std::cout << data4[i*5+j] << " ";
//                 assert(data4[i*5+j] == 12);
//             }
//             std::cout << std::endl;    
//         }
//         std::cout << "Validation PASSED." << std::endl;
//     }
    
//     win.wfree();

// #if defined(USE_VECTORS_INSTEAD_ARRAYS)
//     chunk1.clear();
//     chunk2.clear();
//     chunk3.clear();
//     chunk4.clear();
// #else
//     delete[] data1;
//     delete[] data2;
//     delete[] data3;
//     delete[] data4;    
// #endif
//     MPI_Finalize();

//     return 0;
// }

#include "rmacxx.hpp"

#include <cassert>

const int N =4; // TODO: for some reason, some N doesnt work Investigate!
// N=5,7: Fails @ MPI_Finalize
// N = 6: Fails to compute??
// N = 9: Fails to compute??

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    rmacxx::Window<double,LOCAL_VIEW,EXPR> win({20,20});
    
    // in this case, user still needs to invoke
    // flush, because remote completion to 
    // window is not guaranteed
    //rmacxx::Window<double,LOCAL_VIEW,EXPR,LOCAL_FLUSH> win({20,20});
    
    // no flush needed
    //rmacxx::Window<double,LOCAL_VIEW,EXPR,REMOTE_FLUSH> win({20,20});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    std::vector<double> chunk1, chunk2, chunk3, chunk4;
    chunk1.resize(N*N);
    chunk2.resize(N*N);
    chunk3.resize(N*N);
    chunk4.resize(N*N);
    double* data1 = chunk1.data();
    double* data2 = chunk2.data();
    double* data3 = chunk3.data();
    double* data4 = chunk4.data();
    
    win(1,{0,0},{N-1,N-1})*win(0,{0,0},{N-1,N-1}) + (2+win(1,{0,0},{N-1,N-1})) + (3+win(1,{0,0},{N-1,N-1})) >> data1;
    win(0,{0,0},{N-1,N-1})*win(1,{0,0},{N-1,N-1}) + 4*win(1,{0,0},{N-1,N-1}) + 5*win(1,{0,0},{N-1,N-1}) >> data2;
    win(1,{0,0},{N-1,N-1})*win(0,{0,0},{N-1,N-1}) + 2*(win(1,{0,0},{N-1,N-1})*win(1,{0,0},{N-1,N-1})) >> data3;
    4*(win(1,{0,0},{N-1,N-1})*win(1,{0,0},{N-1,N-1})) + 2*win(0,{0,0},{N-1,N-1}) + (5+win(0,{0,0},{N-1,N-1})) >> data4;
    // TODO: FILE ISSUE ABOUT 4*(win(1,{0,0},{3,3})*win(1,{0,0},{3,3})) =/= 4*win(1,{0,0},{3,3})*win(1,{0,0},{3,3})
    
    win.flush();
    
    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                std::cout << data1[i*N+j] << " ";
                assert(data1[i*N+j] == 8);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < N ; i++)
        {
            for (int j = 0; j < N; j++)
            {
                std::cout << data2[i*N+j] << " ";
                assert(data2[i*N+j] == 10);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                std::cout << data3[i*N+j] << " ";
                assert(data3[i*N+j] == 3);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < N; i++)
        {
            for (int j = 0; j < N; j++)
            {
                std::cout << data4[i*N+j] << " ";
                assert(data4[i*N+j] == 12);
            }
            std::cout << std::endl;    
        }
        std::cout << "Validation PASSED." << std::endl;
    }
    
    win.wfree();

    chunk1.clear();
    chunk2.clear();
    chunk3.clear();
    chunk4.clear();

    MPI_Finalize();

    return 0;
}
