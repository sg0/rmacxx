#pragma once
#ifndef RMACXX_HPP
#define RMACXX_HPP

#include <vector>
#include <iostream>
#include <memory>
#include <algorithm>
#include <string>
#include <cstring>

#ifdef RMACXX_USE_SPINLOCK
#include <atomic>
std::atomic_flag locked = ATOMIC_FLAG_INIT;
#else
#include <mutex>
#endif

#include <functional>
#include <cstdint>
#include <type_traits>
#include <typeinfo>
#include <array>
#include <new>

#include "mpi.h"

#include "util.hpp"

namespace rmacxx
{
// forward declarations for element-wise/bulk
// expressions
template <typename T, class V> class EExpr;
template <typename T, class V> class BExpr;
template <typename T, class P> class RefEExpr;
template <typename T, class P> class RefBExpr;

// tag dispatch to differentiate functions
struct X {};
struct Y {};

// for expressions
template <typename T> struct id { typedef T type; };


// base class for element-wise and bulk expressions
// https://stackoverflow.com/questions/300986/when-should-you-not-use-virtual-destructors
template <typename T> class EExprBase
{
public:
    virtual T eval() const = 0;
    virtual void eexpr_outstanding_put( const T val ) const = 0;

    //virtual ~EExprBase() {}
};

template <typename T> class BExprBase
{
public:
    virtual int fillInto( T* buf ) const = 0;
    virtual void bexpr_outstanding_put( const T* buf ) const = 0;

    //virtual ~BExprBase() {}
};

template <typename T> struct EExprHandle
{
    EExprBase<T>* that_;
    T* result_;
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
    EExprHandle( EExprBase<T>* that, T* result ):
        that_( that ), result_( result )
    {}
    EExprHandle( EExprBase<T>* that ):
        that_( that ), result_( nullptr )
    {}
    EExprHandle( T* result ):
        that_( nullptr ), result_( result )
    {}
#else
    bool is_placed_;

    EExprHandle( EExprBase<T>* that, T* result, bool is_placed ):
        that_( that ), result_( result ), is_placed_( is_placed )
    {}
    EExprHandle( EExprBase<T>* that, bool is_placed ):
        that_( that ), result_( nullptr ), is_placed_( is_placed )
    {}
    EExprHandle( T* result, bool is_placed ):
        that_( nullptr ), result_( result ), is_placed_( is_placed )
    {}
    EExprHandle( T* result ):
        that_( nullptr ), result_( result ), is_placed_( false )
    {}
#endif
};

template <typename T> struct BExprHandle
{
    BExprBase<T>* that_;
    T* result_;
#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
    BExprHandle( BExprBase<T>* that, T* result ):
        that_( that ), result_( result )
    {}
    BExprHandle( BExprBase<T>* that ):
        that_( that ), result_( nullptr )
    {}
    BExprHandle( T* result ):
        that_( nullptr ), result_( result )
    {}
#else
    bool is_placed_;

    BExprHandle( BExprBase<T>* that, T* result, bool is_placed ):
        that_( that ), result_( result ), is_placed_( is_placed )
    {}
    BExprHandle( BExprBase<T>* that, bool is_placed ):
        that_( that ), result_( nullptr ), is_placed_( is_placed )
    {}
    BExprHandle( T* result, bool is_placed ):
        that_( nullptr ), result_( result ), is_placed_( is_placed )
    {}
    BExprHandle( T* result ):
        that_( nullptr ), result_( result ), is_placed_( false )
    {}
#endif
};

// singleton for handles
// tracks objects for deferring
// function calls for expression
// evaluation
template <typename T> class Handles
{
public:
    static Handles& instance()
    {
        static Handles s_instance;
        return s_instance;
    }

    // thwart default copy ctors
    Handles( Handles const& ) = delete;
    void operator=( Handles const& ) = delete;

    inline void* get_eexpr_ptr( int next )
    {
        int& index = eexpr_next_;
        void* ptr = reinterpret_cast<void*>( &eexpr_buf_[index] );
        index += next;
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
#if defined(RMACXX_EXPR_THROW_BAD_ALLOC)

        if ( index >= DEFAULT_EEXPR_SIZE )
            throw std::bad_alloc();

#endif
#else

        if ( index >= DEFAULT_EEXPR_SIZE )
        {
            index -= next;
            return nullptr;
        }

#endif
        return ptr;
    }

    inline void* get_bexpr_ptr( int next )
    {
        int& index = bexpr_next_;
        void* ptr = reinterpret_cast<void*>( &bexpr_buf_[index] );
        index += next;
#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
#if defined(RMACXX_EXPR_THROW_BAD_ALLOC)

        if ( index >= DEFAULT_BEXPR_SIZE )
            throw std::bad_alloc();

#endif
#else

        if ( index >= DEFAULT_BEXPR_SIZE )
        {
            index -= next;
            return nullptr;
        }

#endif
        return ptr;
    }

    inline void eexpr_clear()
    {
        eexpr_next_ = 0;
        eexpr_handles_.clear();
    }

    inline void bexpr_clear()
    {
        bexpr_next_ = 0;
        bexpr_handles_.clear();
    }

    // frees buffer once and for all
    inline void dealloc()
    {
        free( eexpr_buf_ );
        free( bexpr_buf_ );
    }

    // hold objects for element-wise
    // and/or bulk expressions
    std::vector<EExprHandle<T>> eexpr_handles_;
    std::vector<BExprHandle<T>> bexpr_handles_;
private:
    int eexpr_next_, bexpr_next_;
    char* eexpr_buf_;
    char* bexpr_buf_;

    // ctor (invoked once)
    Handles():
        eexpr_next_( 0 ),
        bexpr_next_( 0 ),
        eexpr_buf_( static_cast<char*>( malloc( DEFAULT_EEXPR_SIZE ) ) ),
        bexpr_buf_( static_cast<char*>( malloc( DEFAULT_BEXPR_SIZE ) ) )
    {}
};

// class base templates

// specialized classes will accept base
// template's default args
template <typename T,
          WinType wtype_         = LOCAL_VIEW,
          WinUsage wuse_         = NO_EXPR,
          WinCompletion wcmpl_   = NO_FLUSH,
          WinAtomicity watmc_    = ATOMIC_NONE,
          WinThreadSafety wtsft_ = NOT_CONCURRENT>
class Window {};

// subarray type for local buffer
template <typename T,
          WinType wtype_         = LOCAL_VIEW>
struct RMACXX_Subarray_t {};

#include "rmacxx-local-types.hpp"
#include "rmacxx-mpi-rma.hpp"
#include "rmacxx-local-view.hpp"
#include "rmacxx-global-view.hpp"
#include "rmacxx-expr-elem.hpp"
#include "rmacxx-expr-bulk.hpp"

} // namespace
#endif //RMACXX_HPP
