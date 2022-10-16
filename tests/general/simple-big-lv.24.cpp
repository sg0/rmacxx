#include "rmacxx.hpp"
#include <list>

#include <cassert>

int main(int argc, char *argv[])
{

	MPI_Init(&argc, &argv);
    double t0 = MPI_Wtime();

	int num1, num2, num3, num4, num5;

    rmacxx::Window<int, LOCAL_VIEW> win({10,10});
    win.fill(1);

    std::vector<std::initializer_list<int>> starts({
        {1,2},
        {4,2},
        {2,6},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {1,2},
        {4,2},
        {2,6},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2},
        {0,2}
    });
    std::vector<std::initializer_list<int>> ends({
        {2,6},
        {5,2},
        {4,6},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {2,6},
        {5,2},
        {4,6},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
        {3,3},
    });

    std::vector<int> start(starts[win.rank()]);
    std::vector<int> end(ends[win.rank()]);

    int size = (end[0]-start[0]+1)*(end[1]-start[1]+1);

    std::vector<int> buff(size);
    std::fill(buff.begin(), buff.end(), 3);
    win(win.rank(), starts[win.rank()],ends[win.rank()]) << buff.data();
    std::vector<std::vector<int>> all_results(24);
    for(int rank = 0; rank < 24; rank++){
        std::vector<int> start(starts[rank]);
        std::vector<int> end(ends[rank]);

        int size = (end[0]-start[0]+1)*(end[1]-start[1]+1);
        std::vector<int> results(size);
        win(rank,starts[rank],ends[rank]) >> results.data();
        all_results.push_back(results);

        double t1 = MPI_Wtime();
        std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;

    }
    
    win.flush();

    win.print("After put...");      
    if(win.rank() == 0){
        bool all_threes = true;
        for(auto &vec: all_results) {
            for (auto result: vec) {
                all_threes = all_threes && result == 3;
            }
        }
        assert(all_threes);
    }

    win.wfree();
    MPI_Finalize();

	return 0;
}
