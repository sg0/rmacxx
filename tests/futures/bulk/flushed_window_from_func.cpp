#define RMACXX_USE_FUTURES
#include "rmacxx.hpp"

#include <cassert>

void scale_win(rmacxx::Window<double,LOCAL_VIEW,EXPR>& window){
    rmacxx::Window<double,LOCAL_VIEW,EXPR> other({20,20});
    other.fill(1);
    1*(other(0,{0 ,0 },{9 ,9 }) + window(0,{0 ,0 },{9 ,9 })) >> window(1,{0 ,0 },{9,9});
    2*(other(0,{10,10},{19,19}) + window(0,{10,10},{19,19})) >> window(1,{10,10},{19,19});
    // 3*(other(1,{10,0 },{19, 9}) + window(0,{10,0 },{19, 9})) >> window(1,{10,0 },{19,9});
    // 4*(other(1,{0, 10},{9 ,19}) + window(0,{0, 10},{9 ,19})) >> window(1,{0, 10},{9,19});
    3*(other(0,{10,0 },{19, 9}) + window(0,{10,0 },{19, 9})) >> window(1,{10,0 },{19,9});
    4*(other(0,{0, 10},{9 ,19}) + window(0,{0, 10},{9 ,19})) >> window(1,{0, 10},{9,19});
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

    std::vector<double> chunk1(10*10), chunk2(10*10), chunk3(10*10), chunk4(10*10);
    double* data1 = chunk1.data();
    double* data2 = chunk2.data();
    double* data3 = chunk3.data();
    double* data4 = chunk4.data();

    scale_win(window);
    window.flush();

    window(1,{0 ,0 },{9 ,9 }) >> data1;
    window(1,{10,10},{19,19}) >> data2;
    // window(1,{10,0 },{19, 9}) >> data3;
    // window(1,{0, 10},{9 ,19}) >> data4;
    window(1,{10,0 },{19, 9}) >> data3;
    window(1,{0, 10},{9 ,19}) >> data4;
    
    window.flush();
    
    if (window.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                std::cout << data1[i*5+j] << " ";
                assert(data1[i*5+j] == 2);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                std::cout << data2[i*5+j] << " ";
                assert(data2[i*5+j] == 4);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                std::cout << data3[i*5+j] << " ";
                assert(data3[i*5+j] == 6);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i <5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                std::cout << data4[i*5+j] << " ";
                assert(data4[i*5+j] == 8);
            }
            std::cout << std::endl;    
        }
        std::cout << "Validation PASSED." << std::endl;
    }
    
    window.wfree();

    chunk1.clear();
    chunk2.clear();
    chunk3.clear();
    chunk4.clear();
    MPI_Finalize();

    return 0;
}
