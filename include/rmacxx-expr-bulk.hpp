// expression class that takes
// a reference (to a window)
template <typename T, class P>
class RefBExpr
{
public:
    RefBExpr( P const& p ) : p_( p ) {}

    inline bool is_win_b() const { return p_.is_win_b(); }
    inline T operator()( int idx ) const { return p_( idx ); }
    inline int get_count() const { return p_.get_count(); }
    inline WinCompletion completion() const { return p_.completion(); }
    inline void flush_expr() const { return p_.flush_expr(); }
    inline void flush_win() const { return p_.flush_win(); }

    inline void bexpr_outstanding_gets() const
    { p_.bexpr_outstanding_gets(); }
    inline void bexpr_outstanding_put( const T* buf ) const
    { p_.bexpr_outstanding_put( buf ); }
    inline void expr_ignore_last_get() const
    { p_.expr_ignore_last_get(); }

    int fillInto( T* buf ) const { return p_.fillInto( buf ); }
private:
    P const& p_;
};

template <typename T, class V>
class BExpr : public BExprBase<T>
{
public:
    // ctors
    explicit BExpr( V v ) : v_( v ) {}

    inline bool is_win_b() const { return v_.is_win_b(); }
    inline T operator()( int idx ) const { return v_( idx ); }
    inline int get_count() const { return v_.get_count(); }
    inline WinCompletion completion() const { return v_.completion(); }
    inline void flush_expr() const { return v_.flush_expr(); }
    inline void flush_win() const { return v_.flush_win(); }

    inline void bexpr_outstanding_gets() const
    { v_.bexpr_outstanding_gets(); }
    inline void bexpr_outstanding_put( const T* buf ) const
    { v_.bexpr_outstanding_put( buf ); }
    inline void expr_ignore_last_get() const
    { v_.expr_ignore_last_get(); }

    int fillInto( T* buf ) const { return v_.fillInto( buf ); }

    inline static void flush()
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-bulk.hpp: flush: 54]"<<std::endl;
#endif
        if ( !Handles<T>::instance().bexpr_handles_.empty() )
        {
            for ( unsigned int i = 0; i < Handles<T>::instance().bexpr_handles_.size(); i++ )
            {
                // condition as RHS can be a window, or a POD type
                // in this case, the first instance will always be
                // object and result address for performing a "get",
                // hence result of the previous entry is used as the input
                if ( Handles<T>::instance().bexpr_handles_[i].result_ == nullptr )
                {
                    Handles<T>::instance().bexpr_handles_[i].that_->bexpr_outstanding_put
                    ( Handles<T>::instance().bexpr_handles_[i-1].result_ );
                    
                    // can delete buffer now, as put is
                    // implicitly locally complete

#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
                    Handles<T>::instance().bexpr_handles_[i-1].result_->~T();
#else
                    if ( Handles<T>::instance().bexpr_handles_[i-1].is_placed_ )
                        Handles<T>::instance().bexpr_handles_[i-1].result_->~T();
                    else
                        delete []Handles<T>::instance().bexpr_handles_[i-1].result_;

#endif

#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
                    Handles<T>::instance().bexpr_handles_[i].that_->~BExprBase<T>();
#else

                    if ( Handles<T>::instance().bexpr_handles_[i].is_placed_ )
                        Handles<T>::instance().bexpr_handles_[i].that_->~BExprBase<T>();
                    else
                        delete Handles<T>::instance().bexpr_handles_[i].that_;

#endif
                }
                else
                {
                    if ( Handles<T>::instance().bexpr_handles_[i].that_ != nullptr )
                    {
                        Handles<T>::instance().bexpr_handles_[i].that_->fillInto
                        ( Handles<T>::instance().bexpr_handles_[i].result_ );
                        
                        // delete allocated BExpr ptr
#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
                        Handles<T>::instance().bexpr_handles_[i].that_->~BExprBase<T>();
#else

                        if ( Handles<T>::instance().bexpr_handles_[i].is_placed_ )
                            Handles<T>::instance().bexpr_handles_[i].that_->~BExprBase<T>();
                        else
                            delete Handles<T>::instance().bexpr_handles_[i].that_;

#endif
                    }
                }
            }

            Handles<T>::instance().bexpr_clear();
        }
    }

    // user managed buffer in the RHS
    inline void operator >>( T* buf )
    {
        // post outstanding gets
        bexpr_outstanding_gets();

        if ( is_win_b() )
            fillInto( buf );
        else
        {
            // need to store v_, which is an intermediate
            // object, and will go out of scope if not stored

            // try to use preallocated store for intermediate object

#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
            Handles<T>::instance().bexpr_handles_.
            emplace_back( new ( static_cast<BExpr<T,V>*>( Handles<T>::instance().
                                get_bexpr_ptr( sizeof( BExpr<T,V> ) ) ) ) BExpr<T,V>( v_ ), buf );
#else
            BExpr<T,V>* mem = static_cast<BExpr<T,V>*>
                              ( Handles<T>::instance().get_bexpr_ptr( sizeof( BExpr<T,V> ) ) );

            if ( mem == nullptr )
            {
                Handles<T>::instance().bexpr_handles_.
                emplace_back( new BExpr<T,V>( v_ ), buf, false );
            }
            else
            {
                Handles<T>::instance().bexpr_handles_.
                emplace_back( new ( mem ) BExpr<T,V>( v_ ), buf, true );
            }

#endif
        }
    }

    // user managed window in the RHS
    template <class W>
    void operator >>( BExpr<T,W> const& win )
    {
        bool is_placed = true;

        // ignore previous get
        win.expr_ignore_last_get();
        // post remaining gets for current
        // object
        bexpr_outstanding_gets();
        
        // buffer for storing results of expression
        // and origin buffer for put
        T* buf = static_cast<T*>( Handles<T>::instance().get_bexpr_ptr( sizeof( T )*win.get_count() ) );

#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
#else
        if ( buf == nullptr )
        {
            buf = new T[win.get_count()];
            is_placed = false;
        }

#endif

        // finish evaluation or store current object
        // for later evaluation
        if ( win.is_win_b() )
            fillInto( buf );
        else
        {
#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
            Handles<T>::instance().bexpr_handles_.
            emplace_back( new ( static_cast<BExpr<T,V>*>( Handles<T>::instance().
                                get_bexpr_ptr( sizeof( BExpr<T,V> ) ) ) ) BExpr<T,V>( v_ ), buf );
#else
            BExpr<T,V>* mem = static_cast<BExpr<T,V>*>
                              ( Handles<T>::instance().get_bexpr_ptr( sizeof( BExpr<T,V> ) ) );

            if ( mem == nullptr )
            {
                Handles<T>::instance().bexpr_handles_.
                emplace_back( new BExpr<T,V>( v_ ), buf, false );
            }
            else
            {
                Handles<T>::instance().bexpr_handles_.
                emplace_back( new ( mem ) BExpr<T,V>( v_ ), buf, true );
            }

#endif
        }

        // issue put, user will need to call flush
        // if completion is !REMOTE_FLUSH
        if ( win.completion() == REMOTE_FLUSH )
        {
            win.bexpr_outstanding_put( buf );
            
            win.flush_win(); // for MPI_Win_flush_all
            win.flush_expr();
            
            // buf_ is otherwise deleted during flush, which
            // is not called when REMOTE_FLUSH is specified
            if (!is_placed)
                delete []buf;
        }
        else // store window object
        {
            // stage expression intermediate result
            if ( win.is_win_b() )
            {
#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
                Handles<T>::instance().bexpr_handles_.emplace_back( buf );
#else
                Handles<T>::instance().eexpr_handles_.emplace_back( buf, is_placed );
#endif
            }

#if defined(RMACXX_BEXPR_USE_PLACEMENT_NEW_ALWAYS)
            Handles<T>::instance().bexpr_handles_.
            emplace_back( new ( static_cast<BExpr<T,W>*>( Handles<T>::instance().
                                get_bexpr_ptr( sizeof( BExpr<T,W> ) ) ) ) BExpr<T,W>( win ) );
#else
            // need to store win on heap if required
            BExpr<T,W>* mem = static_cast<BExpr<T,W>*>
                              ( Handles<T>::instance().get_bexpr_ptr( sizeof( BExpr<T,W> ) ) );

            if ( mem == nullptr )
            {
                Handles<T>::instance().bexpr_handles_.
                emplace_back( new BExpr<T,W>( win ), false );
            }
            else
            {
                Handles<T>::instance().bexpr_handles_.
                emplace_back( new ( mem ) BExpr<T,W>( win ), true );
            }
#endif
        }
    }
private:
    V v_;
};

// Window-scalar element-wise operation
template <typename T, class U, class OP>
class BExprWinScalarOp
{
public:
    explicit BExprWinScalarOp( U u, T c ): u_( u ), c_( c ), c_left_( false ) {}
    explicit BExprWinScalarOp( T c, U u ): u_( u ), c_( c ), c_left_( true ) {}

    inline T operator()( int idx ) const
    {
        if ( c_left_ ) return OP::apply( c_, u_( idx ) );

        return OP::apply( u_( idx ), c_ );
    }

    inline void bexpr_outstanding_gets() const { u_.bexpr_outstanding_gets(); }

    inline int get_count() const { return u_.get_count(); }
    inline bool is_win_b() const { return u_.is_win_b(); }
    inline T* get_expr_bptr() { return u_.get_expr_bptr(); }

    int fillInto( T* buf ) const
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-bulk.hpp: fillInto: 295]"<<std::endl;
#endif
        int count = u_.fillInto( buf );

#ifdef DEBUG
        std::cout<< "count: " << count <<std::endl;
#endif
        for ( int i = 0; i < count; i++ ){
            buf[i] = operator()( i );
#ifdef DEBUG
             std::cout<< "i: " << i << "\tbuf[i]: "<< buf[i] <<std::endl;
#endif
        }
        return count;
    }

    // thwart compiler errors
    void bexpr_outstanding_put( const T* buf ) const {}
    inline WinCompletion completion() const { return INVALID_FLUSH; };
    inline void flush_expr() const {}
    inline void flush_win() const {}

private:
    U u_;
    T c_;
    bool c_left_;
};

// window element-wise operations
template <typename T, class U, class V, class OP>
class BExprWinElemOp
{
public:
    explicit BExprWinElemOp( U u, V v ): u_( u ), v_( v ) {}

    inline void bexpr_outstanding_gets() const
    {
        u_.bexpr_outstanding_gets();
        v_.bexpr_outstanding_gets();
    }

    inline T operator()( int idx ) const
    { return OP::apply( u_( idx ), v_( idx ) ); };

    inline bool is_win_b() const
    {
        if ( !u_.is_win_b() || !v_.is_win_b() )
            return false;

        return true;
    }

    inline int get_count() const { return u_.get_count(); }
    
    int fillInto( T* buf ) const
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-bulk.hpp: fillInto: 346]"<<std::endl;
#endif
        T* cache = nullptr;
        const int count = v_.fillInto( buf );

        // try to use placement_new buf
        T* mem = static_cast<T*>( Handles<T>::instance().get_bexpr_ptr( sizeof( T )*count ) );

        if ( mem == nullptr )
            cache = new T[count];
        else
            cache = new ( mem ) T;
        
        // u_'s data is not in preallocated
        // buffer, hence we need to get it
        u_.fillInto( cache );

#ifdef DEBUG
        std::cout<< "count: " << count <<std::endl;
#endif
        for ( int i = 0; i < count; i++ ){
            buf[i] = operator()( i );
#ifdef DEBUG
             std::cout<< "i: " << i << "\tbuf[i]: "<< buf[i] <<std::endl;
#endif
        }
        if ( mem == nullptr )
            delete []cache;
        else
            cache->~T();

        return count;
    }

    // thwart compiler errors
    void bexpr_outstanding_put( const T* buf ) const {}
    inline WinCompletion completion() const { return INVALID_FLUSH; };
    inline void flush_expr() const {}
    inline void flush_win() const {}

private:
    U u_;
    V v_;
};

// s/A
template <typename T, class U>
BExpr <T, BExprWinScalarOp<T, BExpr<T,U>, Div<T>>>
operator/( typename id<T>::type c, BExpr<T,U> u )
{
    typedef BExprWinScalarOp <T, BExpr<T,U>, Div<T>> DivT;
    return BExpr<T, DivT>( DivT( c,u ) );
}

// A/s
template <typename T, class U>
BExpr <T, BExprWinScalarOp<T,BExpr<T,U>, Div<T>>>
operator/( BExpr<T,U> u, typename id<T>::type c )
{
    typedef BExprWinScalarOp <T, BExpr<T,U>, Div<T>> DivT;
    return BExpr<T, DivT>( DivT( u,c ) );
}

// s+A
template <typename T, class U>
BExpr <T, BExprWinScalarOp<T, BExpr<T,U>, Add<T>>>
operator +( typename id<T>::type c, BExpr<T,U> u )
{
    typedef BExprWinScalarOp <T, BExpr<T,U>, Add<T>> SumT;
    return BExpr<T, SumT>( SumT( c,u ) );
}
// A+s
template <typename T, class U>
BExpr <T, BExprWinScalarOp<T,BExpr<T,U>, Add<T>>>
operator+( BExpr<T,U> u, typename id<T>::type c )
{
    typedef BExprWinScalarOp <T, BExpr<T,U>, Add<T>> SumT;
    return BExpr<T, SumT>( SumT( u,c ) );
}
// s*A
template <typename T, class U>
BExpr <T, BExprWinScalarOp<T, BExpr<T,U>, Mul<T>>>
operator*( typename id<T>::type c, BExpr<T,U> u )
{
    typedef BExprWinScalarOp <T, BExpr<T,U>, Mul<T>> MulT;
    return BExpr<T, MulT>( MulT( c,u ) );
}

// A*s
template <typename T, class U>
BExpr <T, BExprWinScalarOp <T, BExpr<T,U>, Mul<T>>>
operator*( BExpr<T,U> u, typename id<T>::type c )
{
    typedef BExprWinScalarOp <T, BExpr<T,U>, Mul<T>> MulT;
    return BExpr<T, MulT>( MulT( u,c ) );
}
// A + B
template <typename T, class U, class V>
BExpr <T, BExprWinElemOp <T, BExpr<T,U>, BExpr<T,V>, Add<T>>>
operator+( BExpr<T,U> u, BExpr<T,V> v )
{
    typedef BExprWinElemOp <T, BExpr<T,U>, BExpr<T,V>, Add<T>> SumT;
    return BExpr<T, SumT>( SumT( u,v ) );
}

// A * B
template <typename T, class U, class V>
BExpr <T, BExprWinElemOp <T, BExpr<T,U>, BExpr<T,V>, Mul<T>>>
operator*( BExpr<T,U> u, BExpr<T,V> v )
{
    typedef BExprWinElemOp <T, BExpr<T,U>, BExpr<T,V>, Mul<T>> MulT;
    return BExpr<T, MulT>( MulT( u,v ) );
}
