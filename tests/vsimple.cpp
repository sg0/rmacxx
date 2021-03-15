#include "rmacxx.hpp"

int main(int argc, char *argv[])
{

    MPI_Init( &argc, &argv );

    // create window
    //rmacxx::Lwindow<int, LOCAL_FLUSH, ANONE> win({10,10});
    rmacxx::Lwindow<int> win({10,10});

    if (win.size() == 1)
    {
        std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // fill window buffer
    win.fill(1);

    // print shape
    win.print("Current...");
    // create a vector and fill
    std::vector<int> a(15); // 5(h) x 3(w) 
    std::fill(a.begin(), a.end(), 2);
    const int *abuf = a.data();

    // put
    win(1, {3,4},{7,6}) << abuf;
    win.flush(1);
    
    win.print("After put...");

     // acc
    win(1, {3,4},{7,6}, SUM) << abuf;
    win.flush(1);
    win.print("After acc...");
    
    // get
    std::vector<int> b(15); 
    int *bbuf = b.data();
    win(1, {3,4},{7,6}) >> bbuf;  

    win.barrier();

    // FOP
    int prev = win(1,{3,4})++;
    win.print("{3,4} incremented by 1");
    std::cout << win.rank() << ": Previous value in position {3,4}: " << prev << std::endl;
    // test
    /*
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
    // element-wise put
    win(1, {3,4}) << 99;
    win.flush(1);
    win.print("After element-wise put...");
    
    std::vector<int> c(5); //
    std::fill(c.begin(), c.end(), 5);
    const int *lbuf = c.data();
    // put with linear buffer
    win(1, {0,0},{0,4}) << lbuf;
    win.flush(1);
    
    win.print("After put...");
    
    win.wfree();

    a.clear();
    b.clear();
    c.clear();

    MPI_Finalize();

    return 0;
}
