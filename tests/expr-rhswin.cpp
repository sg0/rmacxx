#include "rmacxx.hpp"
#include <cassert>

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    rmacxx::Window<double, EXPR> win({10});
    //rmacxx::Window<double, EXPR, LOCAL_FLUSH> win({10});
    //rmacxx::Window<double, EXPR, REMOTE_FLUSH> win({10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    win.print("Current...");
    
    std::vector<double> nums1(5), nums2(5);

    2*win(1,{0},{4}) >> win(0,{5},{9});
    win(1,{0},{4}) + 3*win(1,{5},{9}) >> win(0,{0},{4});

    win.flush();

    // check results
    win(0,{5},{9}) >> nums1.data();
    win(0,{0},{4}) >> nums2.data();

    win.flush();

    // print 
    if (win.rank() == 0)
    {
        std::cout << "Expression results: " << std::endl;
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            std::cout << nums1[i] << " ";
            assert(nums1[i] == 2);
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        for (int i = 0; i < 5; i++)
        {
            std::cout << nums2[i] << " ";
            assert(nums2[i] == 4);
            std::cout << std::endl;    
        }
        std::cout << "--------------------" << std::endl;
        std::cout << "Validation PASSED." << std::endl;
    }

    win.wfree();

    MPI_Finalize();

    return 0;
}
