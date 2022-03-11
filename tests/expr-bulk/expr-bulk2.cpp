#include "rmacxx.hpp"

#include <cassert>
    
int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    rmacxx::Window<double,LOCAL_VIEW,EXPR> win({10,10});
    //rmacxx::Window<double,EXPR,LOCAL_FLUSH> win({10,10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    double* data1 = new double[5*5];
    double* data2 = new double[5*5];

    2*win(1,{0,0},{4,4})*win(1,{0,0},{4,4}) + (2*win(1,{0,0},{4,4})) + (win(1,{0,0},{4,4})+3) >> data1;
    4*win(1,{0,0},{4,4})*win(1,{0,0},{4,4}) + 5*win(1,{0,0},{4,4}) + 6*win(1,{0,0},{4,4}) >> data2;

    //3*win(1,{0,0},{4,4}) >> data1;
    //4*win(1,{0,0},{4,4}) >> data2;
     
    //3*win(1,{0,0},{4,4}) + 5*win(0,{0,0},{4,4}) >> data1;
    //4*win(1,{0,0},{4,4}) + 7*win(0,{0,0},{4,4}) >> data2;
    
    //win(1,{0,0},{4,4})*win(1,{0,1},{4,5}) + win(1,{0,0},{4,4}) + win(1,{0,0},{4,4}) >> data1;
    //win(1,{0,0},{4,4})*win(1,{0,1},{4,5}) + win(1,{0,0},{4,4}) + win(1,{0,0},{4,4}) >> data2;
    
    //3*win(1,{0,0},{4,4}) + 5*win(0,{0,0},{4,4}) + 2*win(0,{0,0},{4,4}) >> data1;
    //4*win(1,{0,0},{4,4}) + 7*win(0,{0,0},{4,4}) + 6*win(0,{0,0},{4,4}) >> data2;
 
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
                std::cout << data1[i*5+j] << " ";
                assert(data1[i*5+j] == 8);
            }
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 5; j++)
            {
                std::cout << data2[i*5+j] << " ";
                assert(data2[i*5+j] == 15);
            }
            std::cout << std::endl;    
        }
        std::cout << "Validation PASSED." << std::endl;
    }
    
    win.wfree();

    delete []data1;
    delete []data2;

    MPI_Finalize();

    return 0;
}
