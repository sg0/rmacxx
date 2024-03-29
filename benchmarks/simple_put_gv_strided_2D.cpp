#include <cstdio>
#ifdef USE_MPI_RMA
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <mpi.h>
#else
#include "rmacxx.hpp"
#endif

#define DEFAULT_NITERS  100000
#define WARMUP_NITERS   20

#ifdef SDE_PROFILING
#define TRACING_SSC_MARK( MARK_ID )                 \
    __asm__ __volatile__ (                          \
            "\n\t  movl $"#MARK_ID", %%ebx"         \
            "\n\t  .byte 0x64, 0x67, 0x90"          \
            : : : "%ebx","memory" );
#endif

#ifdef USE_MPI_RMA
void put(FILE * output, int max_iters, MPI_Comm comm, MPI_Win win)
#else
void put(FILE * output, int max_iters, MPI_Comm comm, rmacxx::Window<char,GLOBAL_VIEW>& win)
#endif
{
    int comm_rank = -1, world_rank = -1, comm_size = 0;
    const char localval = '0';

    // metadata 
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#ifdef USE_MPI_RMA
    MPI_Comm_rank(comm, &comm_rank);
    MPI_Comm_size(comm, &comm_size);
#else
    comm_rank = win.rank();
    comm_size = win.size();
#endif

    if ( comm_rank == 0 )
    {
#ifdef USE_MPI_RMA
	fprintf(output, "============== MPI-3 RMA ==============\n");
#else
	fprintf(output, "================ RMACXX ==============\n");
#endif
    }

#ifdef TEST_OVERHEAD
    int target = MPI_PROC_NULL;
#else
    int target = (comm_rank+1)%comm_size;
#endif

#ifdef USE_MPI_RMA
    MPI_Datatype udtype;
    int sizes[1] = {1};
    int subsizes[1] = {1};
    int starts[1] = {0};

    MPI_Type_create_subarray(1, sizes, subsizes, starts, 
            MPI_ORDER_C, MPI_CHAR, &udtype);
    
    MPI_Type_commit(&udtype);
#else
    rmacxx::RMACXX_Subarray_t<char,GLOBAL_VIEW> foo( {0,0} /*starts*/, {1,1} /*sizes*/ );
#endif

    MPI_Barrier(comm);

    if (comm_rank==0) {
	fprintf(output, "%8s %29s\n", "", "Put             ");
	fprintf(output, "%29s\n","time (us) bandwidth (MB/s)");
	fprintf(output, "-----------------------------------\n");
	fflush(output);
    }

    double t0, t1, dt, avg;

    for (int i = 0; i < WARMUP_NITERS; i++) 
    {
#ifdef USE_MPI_RMA
	    MPI_Put(localval, 1, udtype, target, 0, 1, MPI_CHAR, win);
#else
	    win({0,1}, {0,1}) << foo( &localval );
#endif
    }

    MPI_Barrier(comm);
    t0 = MPI_Wtime();
#ifdef SDE_PROFILING
    TRACING_SSC_MARK(0x6400);
#endif
    // run for a single iteration with profiling turned on
#ifdef SDE_PROFILING
    for (int i = 0; i < 1; i++) 
#else
    for (int i = 0; i < max_iters; i++) 
#endif
    {
#ifdef USE_MPI_RMA
	    MPI_Put(localval, 1, udtype, target, 0, 1, MPI_CHAR, win);
#else
	    win({0,1}, {0,1}) << foo( &localval );
#endif
    }

#ifdef SDE_PROFILING
    TRACING_SSC_MARK(0x6410);
#endif
    
    t1 = MPI_Wtime();
    dt = t1-t0;
    dt /= max_iters;

#ifdef TEST_OVERHEAD
#else
#ifdef USE_MPI_RMA
    MPI_Win_flush(target, win);
    MPI_Type_free(&udtype);
#else
    win.flush(target);
#endif
#endif
    
    MPI_Reduce(&dt, &avg, 1, MPI_DOUBLE, MPI_SUM, 0, comm);    
    avg /= comm_size;

    if (comm_rank==0) 
    {
        fprintf(output, "%13.4lf %13.4lf\n", 1.0e6*avg, (1.0e-6)/avg);
        fflush(output);
    }
    if (comm_rank==0) 
    {
        fprintf(output, "---------------------------------------------\n");
        fflush(output);
    }

    return;
}

int main(int argc, char * argv[])
{
    int world_size = 0, world_rank = -1;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &world_rank );

    double t0 = MPI_Wtime();

    MPI_Comm_size( MPI_COMM_WORLD, &world_size );
    
    int max_iters = (argc>1 ? atoi(argv[1]) : DEFAULT_NITERS);
    if (world_rank==0) printf("Simple put running on %d ranks for %d iterations. \n", world_size, max_iters);

#ifdef USE_MPI_RMA
    MPI_Win win;
    char *base;
    MPI_Win_allocate(sizeof(char), sizeof(char), MPI_INFO_NULL, MPI_COMM_WORLD, &base, &win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
#else
    std::vector<int> lo(2), hi(2);
    if (world_rank == 0)
    {
        lo[0] = 0; lo[1] = 0;
        hi[0] = 0; hi[1] = 0;
    }
    else
    {
        lo[0] = 0; lo[1] = 1;
        hi[0] = 0; hi[1] = 1;
    }
    rmacxx::Window<char,GLOBAL_VIEW> win(lo, hi);
#endif

    // if window is declared inside static_win_rma3, hence
    // when it goes out of scope, the dtor of window is
    // called, which attempts to call mpi_finalize...
    put(stdout, max_iters, MPI_COMM_WORLD, win);

    double t1 = MPI_Wtime();
    double dt = t1-t0;
    if (world_rank==0)
       printf("TEST FINISHED SUCCESSFULLY IN %lf SECONDS \n", dt);
    fflush(stdout);

#ifdef USE_MPI_RMA
    MPI_Win_unlock_all(win);
    MPI_Win_free(&win);
#else
    win.wfree();
#endif
    
    MPI_Finalize();
    
    return 0;
}
