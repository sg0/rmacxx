#pragma once
#ifndef UTIL_HPP
#define UTIL_HPP

#include <bitset>
#include <cstdint>

// NOTE: this header is included at global scope by rmacxx.hpp (outside
// namespace rmacxx), exactly as before. It no longer depends on MPI.

typedef std::bitset<8> byte;

// Reduction / accumulate operators
// --------------------------------
// These used to wrap MPI_Op handles. With the NVSHMEM backend, accumulate
// style operations are implemented element-wise on the GPU with NVSHMEM
// atomics (native fetch-add/swap where available, a compare-and-swap loop
// otherwise), so an operator is just a tag.
//
// MAXLOC/MINLOC are not supported (they require pair types).
enum class OpKind : int
{
    NONE = 0,   // "no accumulate requested" (was MPI_OP_NULL)
    SUM,
    PROD,
    MAX,
    MIN,
    REPLACE,
    NO_OP,
    LAND,
    LOR,
    LXOR,
    BAND,
    BOR,
    BXOR
};

struct Op
{
    OpKind op;
    constexpr explicit Op( OpKind k = OpKind::NONE ) : op( k ) {}
    constexpr bool operator==( Op const& o ) const { return op == o.op; }
    constexpr bool operator!=( Op const& o ) const { return op != o.op; }
};

constexpr Op MAX        = Op( OpKind::MAX );
constexpr Op MIN        = Op( OpKind::MIN );
constexpr Op PROD       = Op( OpKind::PROD );
constexpr Op SUM        = Op( OpKind::SUM );
constexpr Op REPLACE    = Op( OpKind::REPLACE );
constexpr Op NO_OP      = Op( OpKind::NO_OP );
constexpr Op LAND       = Op( OpKind::LAND );
constexpr Op LOR        = Op( OpKind::LOR );
constexpr Op LXOR       = Op( OpKind::LXOR );
constexpr Op BAND       = Op( OpKind::BAND );
constexpr Op BOR        = Op( OpKind::BOR );
constexpr Op BXOR       = Op( OpKind::BXOR );

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

#ifndef PREALLOC_BUF_SZ
#define PREALLOC_BUF_SZ     65536
#endif

// for expressions
#ifndef DEFAULT_EXPR_COUNT
#define DEFAULT_EXPR_COUNT  1000
#endif

// placement new buffer sizes
#ifndef DEFAULT_EEXPR_SIZE
#define DEFAULT_EEXPR_SIZE  4194304
#endif
#ifndef DEFAULT_BEXPR_SIZE
#define DEFAULT_BEXPR_SIZE  16777216
#endif

// element-wise arithmetic operations
template <typename T> struct Add {
#ifdef RMACXX_DEBUG_EXPRS
    static const char S='+';
#endif
    static T apply( T l, T r ) { return ( l+r ); }
};
template <typename T> struct Sub {
#ifdef RMACXX_DEBUG_EXPRS
    static const char S='-';
#endif
    static T apply( T l, T r ) { return ( l-r ); }
};
template <typename T> struct Mul {
#ifdef RMACXX_DEBUG_EXPRS
    static const char S='*';
#endif
    static T apply( T l, T r ) { return ( l*r ); }
};
template <typename T> struct Div {
#ifdef RMACXX_DEBUG_EXPRS
    static const char S='/';
#endif
    static T apply( T l, T r ) { return ( l/r ); }
};

#endif
