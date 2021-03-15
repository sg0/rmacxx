/* Simple bw-test demo using RMACXX */
#include <cstdio>

#include "rmacxx.hpp"

void static_win_rma3(FILE * output, MPI_Comm comm, int max_mem, rmacxx::Lwindow<double>& win)
{
    int comm_rank = -1, world_rank = -1, comm_size = 0;
    int maxcount = max_mem/sizeof(double);

    double * localbuffer = nullptr;

    // metadata 
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    comm_rank = win.rank();
    comm_size = win.size();

    if ( comm_rank == 0 )
        fprintf(output, "============== MPI-3 RMA - RMACXX LOCAL VIEW ==============\n");

    // localbuffer for put/acc
    MPI_Alloc_mem(maxcount*sizeof(double), MPI_INFO_NULL, &localbuffer);
#ifdef TEST_OVERHEAD
    int target = MPI_PROC_NULL;
#else
    int target = (comm_rank+1)%comm_size;
#endif
#if DEBUG>0
    fprintf(output, "doing RMA from rank %d to rank %d \n", comm_rank, target);
    fflush(output);
#endif
    MPI_Barrier(comm);

    if (comm_rank==0) 
    {
        fprintf(output, "%8s %29s %29s %29s \n", "",
                "Put             ", "Get             ", "Acc (SUM)       ");
        fprintf(output, "%8s %29s %29s %29s \n", "doubles",
                "time (us) bandwidth (MB/s)", "time (us) bandwidth (MB/s)", "time (us) bandwidth (MB/s)");
        fprintf(output, "------------------------------------------------------------------------------------------------------\n");
        fflush(output);
    }

    for (int count=1; count<maxcount; count*=2)
    {
        double t0, t1, dt[3], avg[3];
        int iter = maxcount/count;
        int errors = 0;

#if DEBUG>0
        fprintf(output, "%d: maxcount = %d, count = %d, iter = %d \n", comm_rank, maxcount, count, iter);
#endif

        /********************************/

        MPI_Barrier(comm);

        /* zero local buffer and window */
        for (int i=0; i<maxcount; i++)
            localbuffer[i] = 0.0;

        win(comm_rank, {0}, {maxcount-1}) >> localbuffer;
        win.flush(comm_rank);

        MPI_Barrier(comm);

        /********************************/
        for (int i=0; i<maxcount; i++)
            localbuffer[i] = 1000.0+target;

        t0 = MPI_Wtime();
        for (int i=0; i<iter; i++) 
        {
#if DEBUG>1
            fprintf(output, "%d: MPI_Put count=%d, disp=%d \n", comm_rank, count, i*count);
#endif
            win(target, {i*count}, {(i*count + count)-1}) << &localbuffer[i * count];
        }
        win.flush(target);
        t1 = MPI_Wtime();
        dt[0] = t1-t0;

        /********************************/
#if 0
        MPI_Get(localbuffer, maxcount, MPI_DOUBLE, target, 0 /* disp */, maxcount, MPI_DOUBLE, bigwin);
        MPI_Win_flush(target, bigwin);
#else
        t0 = MPI_Wtime();
        for (int i=0; i<iter; i++) 
        {
#if DEBUG>1
            fprintf(output, "%d: MPI_Get count=%d, disp=%d \n", comm_rank, count, i*count);
#endif
            win(target, {i*count}, {(i*count + count)-1}) >> &localbuffer[i * count];
        }
        win.flush(target);
        t1 = MPI_Wtime();
        dt[1] = t1-t0;
#endif
#ifndef TEST_OVERHEAD
        errors = 0;
        for (int i=0; i<(iter*count); i++)
            errors += (localbuffer[i] != (1000.0+target));

        if (errors>0) 
        {
            fprintf(output, "MPI_Put correctness check has failed at comm_rank %d! (errors = %d) \n", world_rank, errors);
#if DEBUG>0
            for (int i=0; i<(iter*count); i++)
                fprintf(output, "rank = %d, i = %d, localbuffer[i] = %lf (%lf) \n", world_rank, i, localbuffer[i], (1000.0+target));
#endif
            MPI_Abort(comm, errors);
        }
#endif // end TEST_OVERHEAD
        /********************************/

        MPI_Barrier(comm);

        /* zero local buffer and window */
        for (int i=0; i<maxcount; i++)
            localbuffer[i] = 1000.0+comm_rank;

        win(comm_rank, {0}, {maxcount-1}) >> localbuffer;
        win.flush(comm_rank);

        MPI_Barrier(comm);

        /********************************/
        for (int i=0; i<maxcount; i++)
            localbuffer[i] = 1000.0+target;

        t0 = MPI_Wtime();
        for (int i=0; i<iter; i++) 
        {
#if DEBUG>1
            fprintf(output, "%d: MPI_Acc count=%d, disp=%d \n", comm_rank, count, i*count);
#endif
            win(target, {i*count}, {(i*count + count)-1}, SUM) << &localbuffer[i * count];
        }
        win.flush(target);
        t1 = MPI_Wtime();
        dt[2] = t1-t0;

        win(comm_rank, {0}, {maxcount-1}) >> localbuffer;
        win.flush(target);

#ifndef TEST_OVERHEAD
        errors = 0;
        for (int i=0; i<(iter*count); i++)
            errors += (localbuffer[i] != (2000.0+2*target));

        if (errors>0) 
        {
            fprintf(output, "MPI_Acc correctness check has failed at comm_rank %d! (errors = %d) \n", world_rank, errors);
#if DEBUG>0
            for (int i=0; i<(iter*count); i++)
                fprintf(output, "rank = %d, i = %d, localbuffer[i] = %lf (%lf) \n", world_rank, i, localbuffer[i], (2000.0+2*target));
#endif
            MPI_Abort(comm, errors);
        } 
#endif // end TEST_OVERHEAD

        for (int i=0; i<3; i++) 
            dt[i] /= iter;
        MPI_Reduce(dt, avg, 3, MPI_DOUBLE, MPI_SUM, 0, comm);
        for (int i=0; i<3; i++) 
            avg[i] /= comm_size;
        if (comm_rank==0) {
            fprintf(output, "%8d %14.4lf %14.4lf %14.4lf %14.4lf %14.4lf %14.4lf\n", count,
                    1.0e6*avg[0], (1.0e-6*count*sizeof(double))/avg[0],
                    1.0e6*avg[1], (1.0e-6*count*sizeof(double))/avg[1],
                    1.0e6*avg[2], (1.0e-6*count*sizeof(double))/avg[2]);
            fflush(output);
        }
    }
    if (comm_rank==0) {
        fprintf(output, "------------------------------------------------------------------------------------------------------\n");
        fflush(output);
    }

    MPI_Free_mem(localbuffer);

    return;
}

int main(int argc, char * argv[])
{
    int world_size = 0, world_rank = -1;
    int provided = -1;

    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &world_rank );

    double t0 = MPI_Wtime();

    MPI_Comm_size( MPI_COMM_WORLD, &world_size );
    if (world_rank==0) printf("MPI test program running on %d ranks. \n", world_size);

    int max_mem = (argc>1 ? atoi(argv[1]) : 2*1024*1024);
    MPI_Comm comm = MPI_COMM_WORLD; // user-defined-comm

    // create a window of doubles with non-blocking ops
    // of size maxcount on comm
    int maxcount = (int)(max_mem/sizeof(double));
    rmacxx::Lwindow<double> win({maxcount}, comm);

    // if window is declared inside static_win_rma3, hence
    // when it goes out of scope, the dtor of window is
    // called, which attempts to call mpi_finalize...
    static_win_rma3(stdout, comm, max_mem, win);

    MPI_Barrier( MPI_COMM_WORLD );

    double t1 = MPI_Wtime();
    double dt = t1-t0;
    if (world_rank==0)
        printf("TEST FINISHED SUCCESSFULLY IN %lf SECONDS \n", dt);
    fflush(stdout);

    win.wfree();

    return 0;
}
