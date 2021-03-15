#include <iostream>
#include <memory>
#include <cstdio>
#include <cstring>

#ifdef USE_MPI_RMA
#include "mpi.h"
#else
#include "rmacxx.hpp"
#endif

#define MAX_ALIGNMENT 65536
#define MAX_MSG_SIZE sizeof(uint64_t)
#define BUFSIZE (MAX_MSG_SIZE + MAX_ALIGNMENT)

int     skip = 10;
int     loop = 500;
double  t_start = 0.0, t_end = 0.0;
char    sbuf_orig[BUFSIZE];
char    wbuf_orig[BUFSIZE];
char    cbuf_orig[BUFSIZE];
char    tbuf_orig[BUFSIZE];
void *sbuf=nullptr, *wbuf=nullptr, *tbuf=nullptr, *cbuf=nullptr;

void run_cas(int rank)
{
    // allocate window
    MPI_Win win;
    MPI_Win_allocate(MAX_MSG_SIZE, 1, MPI_INFO_NULL, MPI_COMM_WORLD, wbuf, &win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, win);
    MPI_Barrier(MPI_COMM_WORLD);

    if(rank == 0) 
    {
        for (int i = 0; i < skip + loop; i++) 
        {
            if (i == skip)
                t_start = MPI_Wtime();
            
            MPI_Compare_and_swap(sbuf, cbuf, tbuf, MPI_LONG_LONG, 1, 0, win);
        }
        t_end = MPI_Wtime ();

        // like other microbenchmarks, flush time is excluded
        MPI_Win_flush_all(win);
    }                

    MPI_Barrier(MPI_COMM_WORLD);
    
    if (rank == 0) 
    {
        fprintf(stdout, "%-*d%*.*f\n", 10, 8, 10, 2, (t_end - t_start) * 1.0e6 / loop);
        fflush(stdout);
    }

    MPI_Win_unlock_all(win);
    MPI_Win_free(&win);
}

int main (int argc, char *argv[])
{
    int size, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (size != 2) 
    {
        if(rank == 0)
            std::cout << "This test requires exactly 2 processes" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -99);
    }

    // return source pointers aligned to uint64_t
    std::size_t sz(BUFSIZE);
    void* psbuf = sbuf_orig;
    sbuf = std::align(alignof(uint64_t), sizeof(uint64_t), psbuf, sz);
    memset(sbuf, 0, MAX_MSG_SIZE);

    void* tsbuf = tbuf_orig;
    tbuf = std::align(alignof(uint64_t), sizeof(uint64_t), tsbuf, sz);
    memset(tbuf, 0, MAX_MSG_SIZE);

    void* wsbuf = wbuf_orig;
    wbuf = std::align(alignof(uint64_t), sizeof(uint64_t), wsbuf, sz);
    memset(wbuf, 0, MAX_MSG_SIZE);

    void* csbuf = cbuf_orig;
    cbuf = std::align(alignof(uint64_t), sizeof(uint64_t), csbuf, sz);
    memset(cbuf, 0, MAX_MSG_SIZE);

    if(rank == 0) 
    {
        fprintf(stdout, "CAS");
        fprintf(stdout, "%-*s%*s\n", 10, "# Size", 10, "Latency (us)");
        fflush(stdout);
    }

    // allocate window and run CAS
    run_cas(rank);

    MPI_Finalize();

    return 0;
}
