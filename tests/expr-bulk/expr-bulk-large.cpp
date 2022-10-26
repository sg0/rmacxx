#define PREALLOC_BUF_SZ 100000
#include "rmacxx.hpp"

#include <cassert>
    
int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    //rmacxx::Window<double,EXPR> win({20,20});
    // in this case, user still needs to invoke
    // flush, because remote completion to 
    // window is not guaranteed
    rmacxx::Window<double,LOCAL_VIEW,EXPR> win({1000,1000});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    std::vector<double> chunk1, chunk2, chunk3, chunk4;
    chunk1.resize(51*51);
    chunk2.resize(51*51);
    chunk3.resize(51*51);
    chunk4.resize(51*51);
    double* data1 = chunk1.data();
    double* data2 = chunk2.data();
    double* data3 = chunk3.data();
    double* data4 = chunk4.data();
  
    win(1,{0,0},{50,50})*win(0,{0,0},{50,50}) + (2+win(1,{0,0},{50,50})) + (3+win(1,{0,0},{50,50}))     >> data1;
    win(0,{0,0},{50,50})*win(1,{0,0},{50,50}) + 4*win(1,{0,0},{50,50}) + 5*win(1,{0,0},{50,50})         >> data2;
    win(1,{0,0},{50,50})*win(0,{0,0},{50,50}) + 2*win(1,{0,0},{50,50})*win(1,{0,0},{50,50})             >> data3;
    4*win(1,{0,0},{50,50})*win(1,{0,0},{50,50}) + 2*win(0,{0,0},{50,50}) + (5+win(0,{0,0},{50,50}))     >> data4;

    // flush to complete expression
    win.flush();

    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (j == 4)
                    std::cout << ".";
                else
                    std::cout << data1[i*50+j] << " ";
                assert(data1[i*50+j] == 8);
            }
            if (i == 4)
            {
                std::cout << std::endl;    
                for (int k = 0; k < 5; k++)
                    std::cout << "." << " ";
                std::cout << std::endl;    
            }
            else
                std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (j == 4)
                    std::cout << ".";
                else
                    std::cout << data2[i*50+j] << " ";
                assert(data2[i*50+j] == 10);
            }
            if (i == 4)
            {
                std::cout << std::endl;    
                for (int k = 0; k < 5; k++)
                    std::cout << "." << " ";
                std::cout << std::endl;    
            }
            else
                std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (j == 4)
                    std::cout << ".";
                else
                    std::cout << data3[i*50+j] << " ";
                assert(data3[i*50+j] == 3);
            }
            if (i == 4)
            {
                std::cout << std::endl;    
                for (int k = 0; k < 5; k++)
                    std::cout << "." << " ";
                std::cout << std::endl;    
            }
            else
                std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                if (j == 4)
                    std::cout << ".";
                else
                    std::cout << data4[i*50+j] << " ";
                assert(data4[i*50+j] == 12);
            }
            if (i == 4)
            {
                std::cout << std::endl;    
                for (int k = 0; k < 5; k++)
                    std::cout << "." << " ";
                std::cout << std::endl;    
            }
            else
                std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
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
