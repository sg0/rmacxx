// CPU emulation of the MPI subset used by RMACXX and its tests (testing only)
#pragma once
#include "emu_core.hpp"

typedef int MPI_Comm;
typedef int MPI_Info;
#define MPI_COMM_WORLD 0
#define MPI_COMM_NULL ( -1 )
#define MPI_INFO_NULL 0
#define MPI_COMM_TYPE_SHARED 1
#define MPI_THREAD_MULTIPLE 3
#define MPI_SUCCESS 0
enum { MPI_IDENT = 0, MPI_CONGRUENT = 1, MPI_SIMILAR = 2, MPI_UNEQUAL = 3 };

namespace emu
{
inline thread_local bool tl_mpi_init = false, tl_mpi_fin = false;
}

inline int MPI_Init( int*, char*** ) { emu::tl_mpi_init = true; return 0; }
inline int MPI_Init_thread( int*, char***, int req, int* prov ) { *prov = req; emu::tl_mpi_init = true; return 0; }
inline int MPI_Initialized( int* f ) { *f = emu::tl_mpi_init; return 0; }
inline int MPI_Finalized( int* f ) { *f = emu::tl_mpi_fin; return 0; }
inline int MPI_Finalize() { emu::tl_mpi_fin = true; return 0; }
inline int MPI_Comm_rank( MPI_Comm, int* r ) { *r = emu::tl_pe; return 0; }
inline int MPI_Comm_size( MPI_Comm, int* s ) { *s = emu::world().npes; return 0; }
inline int MPI_Barrier( MPI_Comm ) { emu::barrier(); return 0; }
inline int MPI_Comm_split_type( MPI_Comm, int, int, MPI_Info, MPI_Comm* n ) { *n = 1; return 0; }
inline int MPI_Comm_free( MPI_Comm* c ) { *c = MPI_COMM_NULL; return 0; }
inline int MPI_Comm_compare( MPI_Comm a, MPI_Comm b, int* r ) { *r = ( a == b ) ? MPI_IDENT : MPI_UNEQUAL; return 0; }
inline int MPI_Abort( MPI_Comm, int code ) { std::fflush( stdout ); std::_Exit( code ? code : 1 ); }
inline double MPI_Wtime()
{
    return std::chrono::duration<double>( std::chrono::steady_clock::now().time_since_epoch() ).count() - emu::world().t0;
}
