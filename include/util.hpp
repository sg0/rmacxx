#pragma once
#ifndef UTIL_HPP
#define UTIL_HPP

#include <bitset>
typedef std::bitset<8> byte;

template<typename T> MPI_Datatype TypeMap();
template<> inline MPI_Datatype TypeMap<byte>() { return MPI_UNSIGNED_CHAR; }
template<> inline MPI_Datatype TypeMap<char>() { return MPI_CHAR; }
template<> inline MPI_Datatype TypeMap<short int>() { return MPI_SHORT; }
template<> inline MPI_Datatype TypeMap<int>() { return MPI_INT; }
template<> inline MPI_Datatype TypeMap<long>() { return MPI_LONG; }
template<> inline MPI_Datatype TypeMap<long long>() { return MPI_LONG_LONG_INT; }
template<> inline MPI_Datatype TypeMap<unsigned short>() { return MPI_UNSIGNED_SHORT; }
template<> inline MPI_Datatype TypeMap<unsigned int>() { return MPI_UNSIGNED; }
template<> inline MPI_Datatype TypeMap<unsigned long>() { return MPI_UNSIGNED_LONG; }
template<> inline MPI_Datatype TypeMap<unsigned long long>() { return MPI_UNSIGNED_LONG_LONG; }
/* 
template<> inline MPI_Datatype TypeMap<int8_t>() { return MPI_INT8_T; }
template<> inline MPI_Datatype TypeMap<int16_t>() { return MPI_INT16_T; }
template<> inline MPI_Datatype TypeMap<int32_t>() { return MPI_INT32_T; }
template<> inline MPI_Datatype TypeMap<int64_t>() { return MPI_INT64_T; }
template<> inline MPI_Datatype TypeMap<uint8_t>() { return MPI_INT8_T; }
template<> inline MPI_Datatype TypeMap<uint16_t>() { return MPI_UINT16_T; }
template<> inline MPI_Datatype TypeMap<uint32_t>() { return MPI_UINT32_T; }
template<> inline MPI_Datatype TypeMap<uint64_t>() { return MPI_UINT64_T; }
*/
template<> inline MPI_Datatype TypeMap<float>() { return MPI_FLOAT; }
template<> inline MPI_Datatype TypeMap<double>() { return MPI_DOUBLE; }
template<> inline MPI_Datatype TypeMap<long double>() { return MPI_LONG_DOUBLE; }

struct Op
{
    MPI_Op op;
    explicit Op( MPI_Op mpiop = MPI_OP_NULL ) : op( mpiop ) {}
};

const Op MAX        = static_cast<Op>( MPI_MAX );
const Op MIN        = static_cast<Op>( MPI_MIN );
const Op MAXLOC     = static_cast<Op>( MPI_MAXLOC );
const Op MINLOC     = static_cast<Op>( MPI_MINLOC );
const Op PROD       = static_cast<Op>( MPI_PROD );
const Op SUM        = static_cast<Op>( MPI_SUM );
const Op REPLACE    = static_cast<Op>( MPI_REPLACE );
const Op LAND       = static_cast<Op>( MPI_LAND );
const Op LOR        = static_cast<Op>( MPI_LOR );
const Op LXOR       = static_cast<Op>( MPI_LXOR );
const Op BAND       = static_cast<Op>( MPI_BAND );
const Op BOR        = static_cast<Op>( MPI_BOR );
const Op BXOR       = static_cast<Op>( MPI_BXOR );

// Window attributes
// -----------------
// global or local view
// window
typedef enum
{
    LOCAL_VIEW,
    GLOBAL_VIEW
} WinType;

// Expression
// ----------
typedef enum
{
    NO_EXPR,
    EXPR
} WinUsage;

// Completion
// ----------
typedef enum
{
    NO_FLUSH,
    LOCAL_FLUSH,
    REMOTE_FLUSH,
    INVALID_FLUSH
} WinCompletion;

// Atomicity
// ----------
typedef enum
{
    ATOMIC_NONE,
    ATOMIC_PUT_GET
} WinAtomicity;

// Concurrency
// ----------
typedef enum
{
    NOT_CONCURRENT,
    CONCURRENT
} WinThreadSafety;

// kind (currently unused)
typedef enum
{
    ALLOC,
    CREATE,
    DYNAMIC
} WinKind;

#define PREALLOC_BUF_SZ     256

// for expressions
#define DEFAULT_EXPR_COUNT  1000

// placement new buffer sizes
#define DEFAULT_EEXPR_SIZE  4194304
#define DEFAULT_BEXPR_SIZE  16777216

// element-wise arithmetic operations
template <typename T> struct Add { static T apply( T l, T r ) { return ( l+r ); } };
template <typename T> struct Sub { static T apply( T l, T r ) { return ( l-r ); } };
template <typename T> struct Mul { static T apply( T l, T r ) { return ( l*r ); } };
template <typename T> struct Div { static T apply( T l, T r ) { return ( l/r ); } };

#endif
