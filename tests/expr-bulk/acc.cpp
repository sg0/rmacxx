#include "rmacxx.hpp"

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    //rmacxx::Window<int, LOCAL_FLUSH, ANONE> win({10,10});
    rmacxx::Window<int> win({10,10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    win.barrier();
    win.print("Current...");
    
    // create a vector and fill
    std::vector<int> a(15); // 5(h) x 3(w) 
    std::fill(a.begin(), a.end(), 2);
    const int *abuf = a.data();

    // acc
    win(1, {3,4},{7,6}, SUM) << abuf;
    win.flush(1);
    win.barrier();
    
    win.print("After acc...");

    // test
    /*
    // get
    std::vector<int> b(15); 
    int *bbuf = b.data();
    win(1, {3,4},{7,6}) >> bbuf;  
    std::cout << "After get..." << std::endl;
    for (int i = 0; i < 5; i++)
    {
	for (int j = 0; j < 3; j++)
	{
	    std::cout << " " << b[i*3 + j] << " ";
	}
	std::cout << std::endl;
    }
    */
    
    win.wfree();

    a.clear();

    MPI_Finalize();

    return 0;
}
