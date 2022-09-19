#define BENCHMARK "MPI_Get%s Bandwidth Test"

#ifdef USE_MPI_RMA
#include <mpi.h>
#else
#include "rmacxx.hpp"
#endif

#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>


#define MAX_SIZE (1<<22)

#define SKIP_LARGE  10
#define LOOP_LARGE  30
#define WINDOW_SIZE_LARGE 32
#define LARGE_MESSAGE_SIZE 8192
#define MAX_ALIGNMENT 65536
#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 2
#endif

#define MYBUFSIZE ((MAX_SIZE * WINDOW_SIZE_LARGE) + MAX_ALIGNMENT)

#ifdef PACKAGE_VERSION
#   define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#   define HEADER "# " BENCHMARK "\n"
#endif

int     skip = 20;
int     loop = 100;
double  t_start = 0.0, t_end = 0.0;
char    sbuf_original[MYBUFSIZE];
char    *sbuf=nullptr;

void print_header (int); 
void print_bw (int, int, double);
void run_get_with_flush (int);

int main (int argc, char *argv[])
{
    int         rank,nprocs;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(nprocs != 2) {
	if(rank == 0) {
	    fprintf(stderr, "This test requires exactly two processes\n");
	}

	MPI_Finalize();

	return EXIT_FAILURE;
    }

    print_header(rank);

    run_get_with_flush(rank);

    MPI_Finalize();

    return EXIT_SUCCESS;
}

void print_header (int rank)
{
    if(rank == 0) {

	fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Messages/s");
	fflush(stdout);
    }
}

void print_bw(int rank, int size, double t)
{
    if (rank == 0) {
        double tmp = size / 1e6;
        tmp *= WINDOW_SIZE_LARGE * loop;
        tmp /= t;

        double rate = 1e6 * tmp / size;

        fprintf(stdout,
                "%-*d%*.*f\n", 10, size,
                FIELD_WIDTH,
                FLOAT_PRECISION,
                rate);
        fflush(stdout);
    }
}

void *align_buffer (void * ptr, unsigned long align_size)
{
    return (void *)(((unsigned long)ptr + (align_size - 1)) / align_size *
            align_size);
}

/*Run PUT with flush */
void run_get_with_flush (int rank)
{
    double t;
    int size, i, j;
#ifdef USE_MPI_RMA
    MPI_Win win;
    MPI_Aint disp = 0;
#endif

    int window_size = WINDOW_SIZE_LARGE;
    int page_size;

    page_size = getpagesize();
    assert(page_size <= MAX_ALIGNMENT);

    sbuf = (char *)align_buffer(&sbuf_original, page_size);

    for (size = 1; size <= MAX_SIZE; size = size * 2) {
	memset(sbuf, 'a', size*window_size);

#ifdef USE_MPI_RMA
	char *win_base=nullptr;
	MPI_Win_allocate(size*window_size, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win_base, &win);
#else
        rmacxx::Window<char,LOCAL_VIEW> rwin({size*window_size});
	MPI_Win win = rwin.wunlock();
#endif
	if(size > LARGE_MESSAGE_SIZE) {
	    loop = LOOP_LARGE;
	    skip = SKIP_LARGE;
	}

	if (rank == 0) {
	    MPI_Win_lock(MPI_LOCK_SHARED, 1, 0, win);
	    for (i = 0; i < skip + loop; i++) {
		if (i == skip) {
		    t_start = MPI_Wtime ();
		}
		for(j = 0; j < window_size; j++) {
#ifdef USE_MPI_RMA
		    MPI_Get(sbuf+(j*size), size, MPI_CHAR, 1, disp + (j * size), size, MPI_CHAR, win);
#else
		    rwin(1, {j*size}, {j*size+size-1}) >> sbuf+(j*size);
#endif
		}
#ifdef USE_MPI_RMA
		MPI_Win_flush_local(1, win);
#else
		rwin.flush_local(1);
#endif
	    }
	    t_end = MPI_Wtime();
            MPI_Win_unlock(1, win);
	    t = t_end - t_start;
	}

        // rank 1 blocks here
	MPI_Barrier(MPI_COMM_WORLD);

	print_bw(rank, size, t);

#ifdef USE_MPI_RMA
#else
	rwin.wfree();
#endif
	MPI_Win_free(&win);
    }
}

/* vi: set sw=4 sts=4 tw=80: */
