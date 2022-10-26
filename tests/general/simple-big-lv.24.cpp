
#include "rmacxx.hpp"
#include <list>
#include <tuple>

#include <cassert>

int main(int argc, char *argv[])
{

	MPI_Init(&argc, &argv);
    double t0 = MPI_Wtime();

	int num1, num2, num3, num4, num5;

    rmacxx::Window<int, LOCAL_VIEW> win({10,10});
    win.fill(1);

    std::vector<std::tuple<std::initializer_list<int>,std::initializer_list<int>>> starts_and_ends({
        {{1,2},{2,6}},
        {{4,2},{5,2}},
        {{2,6},{4,6}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{1,2},{2,6}},
        {{4,2},{5,2}},
        {{2,6},{4,6}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
        {{0,2},{3,3}},
    });

    std::tuple<std::initializer_list<int>,std::initializer_list<int>>& my_starts_and_ends = starts_and_ends[win.rank()];

    std::vector<int> start(std::get<0>(my_starts_and_ends));
    std::vector<int> end(std::get<1>(my_starts_and_ends));

    int size = (end[0]-start[0]+1)*(end[1]-start[1]+1);

    std::vector<int> buff(size);
    std::fill(buff.begin(), buff.end(), 3);
    win(win.rank(), std::get<0>(my_starts_and_ends),std::get<1>(my_starts_and_ends)) << buff.data();
    win.flush();
    MPI_Barrier(MPI_COMM_WORLD);
    std::vector<std::vector<int>> all_results;
    all_results.reserve(24); 
    if(win.rank() == 0){
        for(int rank = 0; rank < 24; rank++){
            std::tuple<std::initializer_list<int>,std::initializer_list<int>>& current_starts_and_ends = starts_and_ends[rank];

            std::vector<int> start(std::get<0>(current_starts_and_ends));
            std::vector<int> end(std::get<1>(current_starts_and_ends));

            int size = (end[0]-start[0]+1)*(end[1]-start[1]+1);
            std::vector<int> results(size);
            win(rank,std::get<0>(current_starts_and_ends),std::get<1>(current_starts_and_ends)) >> results.data();
            all_results.push_back(results);
            double t1 = MPI_Wtime();
            std::cout<<"Time elapsed: "<<t1 - t0<<std::endl;
        }
    }
    
    win.flush();

    win.print("After put...");      
    if(win.rank() == 0){
        bool all_threes = true;
        int failed_rank = -1;
        int failed_index = -1;

        int current_rank = 0;
        for(auto &vec: all_results) {         
            int current_index = 0;
            printf("Rank %d: ",current_rank);
            rmacxx::print_vector(vec);
            for (auto result: vec) {
                if(result != 3){
                    all_threes = false;
                    failed_index = current_index;
                    failed_rank = current_rank;
                    break;
                }
                current_index++;
            }
            if(!all_threes){
                break;
            }
            current_rank++;
        }
        current_rank = 0;
        for(auto &vec: all_results) {         
            printf("Rank %d: ",current_rank);
            rmacxx::print_vector(vec);
            current_rank++;
        }
        if(failed_index!=-1){
            printf("FailedRank = %d\n",failed_rank);
            printf("FailedIndex = %d\n",failed_index);
            printf("Value = %d\n",all_results[failed_rank][failed_index]);
        }
        assert(all_threes);
    }

    win.wfree();
    MPI_Finalize();

	return 0;
}
