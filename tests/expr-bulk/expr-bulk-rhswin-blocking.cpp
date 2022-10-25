#include "rmacxx.hpp"

#include <cassert>
    
int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    rmacxx::Window<double,LOCAL_VIEW,EXPR,LOCAL_FLUSH> win({20,20});
    
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
    chunk1.resize(4*4);
    chunk2.resize(4*4);
    chunk3.resize(4*4);
    chunk4.resize(4*4);
    double* data1 = chunk1.data();
    double* data2 = chunk2.data();
    double* data3 = chunk3.data();
    double* data4 = chunk4.data();

    win(1,{0,0},{3,3})*win(0,{0,0},{3,3}) + (2+win(1,{0,0},{3,3})) + (3+win(1,{0,0},{3,3})) >> win(0,{4,4},{7,7});
    win(0,{0,0},{3,3})*win(1,{0,0},{3,3}) + 4*win(1,{0,0},{3,3}) + 5*win(1,{0,0},{3,3}) >> win(1,{8,8},{11,11});
    win(1,{0,0},{3,3})*win(0,{0,0},{3,3}) + 2*(win(1,{0,0},{3,3})*win(1,{0,0},{3,3})) >> win(0,{12,12},{15,15});
    4*(win(1,{0,0},{3,3})*win(1,{0,0},{3,3})) + 2*win(0,{0,0},{3,3}) + (5+win(0,{0,0},{3,3})) >> win(1,{16,16},{19,19});
    // TODO: FILE ISSUE ABOUT 4*(win(1,{0,0},{3,3})*win(1,{0,0},{3,3})) =/= 4*win(1,{0,0},{3,3})*win(1,{0,0},{3,3})

    // flush to complete expression
    win.flush();

    win(0,{4,4},{7,7}) >> data1;
    win(1,{8,8},{11,11}) >> data2;
    win(0,{12,12},{15,15}) >> data3;
    win(1,{16,16},{19,19}) >> data4;   
    
    win.flush();
    
    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                std::cout << data1[i*4+j] << " ";
                assert(data1[i*4+j] == 8);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                std::cout << data2[i*4+j] << " ";
                assert(data2[i*4+j] == 10);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                std::cout << data3[i*4+j] << " ";
                assert(data3[i*4+j] == 3);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                std::cout << data4[i*4+j] << " ";
                assert(data4[i*4+j] == 12);
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
