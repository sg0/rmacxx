#include "rmacxx.hpp"

#include <cassert>

int main(int argc, char *argv[])
{

	MPI_Init(&argc, &argv);

	int num1, num2, num3, num4, num5;

	// create window for expression
	rmacxx::Window<int, LOCAL_VIEW, EXPR> win({1000});
	// rmacxx::Window<int, LOCAL_VIEW, EXPR, REMOTE_FLUSH> win({10});

	if (win.size() == 1)
	{
		std::cout << "Number of processes should be more than 1. Aborting..." << std::endl;
		MPI_Abort(MPI_COMM_WORLD, -99);
	}

	// fill window buffer
	win.fill(1);
	win.print("Current...");

	// expressions...
	win(1, {0}) + 2 * win(1, {500}) + 3 * win(1, {500})	>> win(0, {990});
	win(1, {999}) + win(1, {0})                   		>> win(0, {991});
	win(1, {500}) + 3 * win(1, {4})				 		>> win(1, {992});
	2 * win(1, {0}) + 3 * win(1, {0}) 					>> win(1, {993});
	win(1, {2}) + 2 * win(1, {200}) + 5 * win(1, {0}) 	>> win(1, {994});

	// in this case, a flush is needed to
	// complete the expression evaluations
	win.flush();
	win.print("Current...");

	win(0, {990}) >> num1;
	win(0, {991}) >> num2;
	win(1, {992}) >> num3;
	win(1, {993}) >> num4;
	win(1, {994}) >> num5;
	win.flush();

	if (win.rank() == 0)
	{
		std::cout << "Expression results: " << std::endl;
		std::cout << "------------------- " << std::endl;

		std::cout << "win(1,{0}) + 2*win(1,{0}) + 3*win(1,{0}) = " << num1 << std::endl;
		std::cout << "win(1,{0}) + win(1,{0}) = " << num2 << std::endl;
		std::cout << "win(1,{0}) + 3*win(1,{4}) = " << num3 << std::endl;
		std::cout << "2*win(1,{0}) + 3*win(1,{0}) = " << num4 << std::endl;
		std::cout << "win(1,{2}) + 2*win(1,{0}) + 5*win(1,{0}) = " << num5 << std::endl;
	}

	// check results
	assert(num1 == 6);
	assert(num2 == 2);
	assert(num3 == 4);
	assert(num4 == 5);
	assert(num5 == 8);
	if (win.rank() == 0){
		std::cout << "Validation PASSED." << std::endl;
	}
	MPI_Barrier(MPI_COMM_WORLD);

	win.wfree();

	MPI_Finalize();

	return 0;
}
