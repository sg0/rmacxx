// expression class that takes
// a reference
// required to wrap the window object
// to create an EExpr object
template <typename T, class P>
class RefEExpr
{
public:
    RefEExpr( P const& p ) : p_( p ) {}

    inline T eval() const { return p_.eval(); }
    inline bool is_win_b() const { return p_.is_win_b(); }
    inline WinCompletion completion() const { return p_.completion(); }
    inline void flush_win() const { p_.flush_win(); }
    inline void flush_expr() const { p_.flush_expr(); }

    inline void eexpr_outstanding_gets() const
    { p_.eexpr_outstanding_gets(); }
    inline void eexpr_outstanding_put( const T val ) const
    { p_.eexpr_outstanding_put( val ); }
    inline void expr_ignore_last_get() const
    { p_.expr_ignore_last_get(); }
private:
    P const& p_;
};

template <typename T, class V>
class EExpr : public EExprBase<T>
{
public:
    explicit EExpr( V v ) : v_( v ) {}

    inline T eval() const { return v_.eval(); }
    inline bool is_win_b() const { return v_.is_win_b(); }
    inline WinCompletion completion() const { return v_.completion(); }
    inline void flush_win() const { v_.flush_win(); }
    inline void flush_expr() const { v_.flush_expr(); }

    inline void eexpr_outstanding_gets() const
    { v_.eexpr_outstanding_gets(); }
    inline void eexpr_outstanding_put( const T val ) const
    { v_.eexpr_outstanding_put( val ); }
    inline void expr_ignore_last_get() const
    { v_.expr_ignore_last_get(); }

    inline static void flush()
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: flush: 48]"<<std::endl;
#endif
        if ( !Handles<T>::instance().eexpr_handles_.empty() )
        {
            for ( unsigned int i = 0; i < Handles<T>::instance().eexpr_handles_.size(); i++ )
            {
                // condition as RHS can be a window, or a POD type
                if ( Handles<T>::instance().eexpr_handles_[i].result_ == nullptr )
                {
                    Handles<T>::instance().eexpr_handles_[i].that_->eexpr_outstanding_put
                    ( *( Handles<T>::instance().eexpr_handles_[i-1].result_ ) );
                    
                    // can free buffer now, as _outstanding_put is
                    // implicitly locally complete
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
                    Handles<T>::instance().eexpr_handles_[i-1].result_->~T();
#else
                    if ( Handles<T>::instance().eexpr_handles_[i-1].is_placed_ )
                        Handles<T>::instance().eexpr_handles_[i-1].result_->~T();
                    else
                        delete Handles<T>::instance().eexpr_handles_[i-1].result_;

#endif
                    
                    // delete allocated EExpr ptr
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
                    Handles<T>::instance().eexpr_handles_[i].that_->~EExprBase<T>();
#else

                    if ( Handles<T>::instance().eexpr_handles_[i].is_placed_ )
                        Handles<T>::instance().eexpr_handles_[i].that_->~EExprBase<T>();
                    else
                        delete Handles<T>::instance().eexpr_handles_[i].that_;

#endif
                }
                else
                {
                    if ( Handles<T>::instance().eexpr_handles_[i].that_ != nullptr )
                    {
                        *( Handles<T>::instance().eexpr_handles_[i].result_ )
                            = Handles<T>::instance().eexpr_handles_[i].that_->eval();
                        
                        // delete allocated EExpr ptr
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
                        Handles<T>::instance().eexpr_handles_[i].that_->~EExprBase<T>();
#else

                        if ( Handles<T>::instance().eexpr_handles_[i].is_placed_ )
                            Handles<T>::instance().eexpr_handles_[i].that_->~EExprBase<T>();
                        else
                            delete Handles<T>::instance().eexpr_handles_[i].that_;

#endif
                    }
                }
            }

            Handles<T>::instance().eexpr_clear();
        }
    }

    // >> triggers the evaluation
    inline void operator >>( T& d )
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator >>: 115]"<<std::endl;
#endif
        // post gets
        eexpr_outstanding_gets();

        if ( is_win_b() )
            d = eval();
        else
        {
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
            Handles<T>::instance().eexpr_handles_.
            emplace_back( new ( static_cast<EExpr<T,V>*>( Handles<T>::instance().
                                get_eexpr_ptr( sizeof( EExpr<T,V> ) ) ) ) EExpr<T,V>( v_ ), &d );
#else
            // try to use preallocated store for intermediate object
            EExpr<T,V>* mem = static_cast<EExpr<T,V>*>
                              ( Handles<T>::instance().get_eexpr_ptr( sizeof( EExpr<T,V> ) ) );

            if ( mem == nullptr )
            {
                Handles<T>::instance().eexpr_handles_.
                emplace_back( new EExpr<T,V>( v_ ), &d, false );
            }
            else
            {
                Handles<T>::instance().eexpr_handles_.
                emplace_back( new ( mem ) EExpr<T,V>( v_ ), &d, true );
            }

#endif
        }
    }

    template <class W>
    void operator >>( EExpr<T,W> const& win )
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator >>: 152]"<<std::endl;
#endif
        bool is_placed = true;

        // ignore previous get
        win.expr_ignore_last_get();
        
        // post remaining gets for current
        // object
        eexpr_outstanding_gets();
        T* c = static_cast<T*>( Handles<T>::instance().get_eexpr_ptr( sizeof( T ) ) );

        // try to use preallocated store for intermediate object
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
#else
        if ( c == nullptr )
        {
            c = new T;
            is_placed = false;
        }
#endif

        // finish evaluation or store current object
        // for later evaluation
        if ( win.is_win_b() )
            *c = eval();
        else
        {
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
            Handles<T>::instance().eexpr_handles_.
            emplace_back( new ( static_cast<EExpr<T,V>*>( Handles<T>::instance().
                                get_eexpr_ptr( sizeof( EExpr<T,V> ) ) ) ) EExpr<T,V>( v_ ), c );
#else
            EExpr<T,V>* mem = static_cast<EExpr<T,V>*>
                              ( Handles<T>::instance().get_eexpr_ptr( sizeof( EExpr<T,V> ) ) );

            if ( mem == nullptr )
            {
                Handles<T>::instance().eexpr_handles_.
                emplace_back( new EExpr<T,V>( v_ ), c, false );
            }
            else
            {
                Handles<T>::instance().eexpr_handles_.
                emplace_back( new ( mem ) EExpr<T,V>( v_ ), c, true );
            }

#endif
        }

        // issue put, user will need to call flush
        // if completion is !REMOTE_FLUSH
        
        if ( win.completion() == REMOTE_FLUSH )
        {
            win.eexpr_outstanding_put( *c );
            
            win.flush_win();
            win.flush_expr();
            
            if (!is_placed)
                delete c;
        }
        else // store window object
        {
            // stage expression intermediate result
            // if LOCAL_FLUSH
            if ( win.is_win_b() )
            {
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
                Handles<T>::instance().eexpr_handles_.emplace_back( c );
#else
                Handles<T>::instance().eexpr_handles_.emplace_back( c, is_placed );

#endif
            }

#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
            Handles<T>::instance().eexpr_handles_.
            emplace_back( new ( static_cast<EExpr<T,W>*>( Handles<T>::instance().
                                get_eexpr_ptr( sizeof( EExpr<T,W> ) ) ) ) EExpr<T,W>( win ) );
#else
            EExpr<T,W>* mem = static_cast<EExpr<T,W>*>
                              ( Handles<T>::instance().get_eexpr_ptr( sizeof( EExpr<T,W> ) ) );

            if ( mem == nullptr )
            {
                Handles<T>::instance().eexpr_handles_.
                emplace_back( new EExpr<T,W>( win ), false );
            }
            else
            {
                Handles<T>::instance().eexpr_handles_.
                emplace_back( new ( mem ) EExpr<T,W>( win ), true );
            }

#endif
        }
    }
private:
    V v_;
};

template <typename T, class A, class B, class OP>
class EExprWinElemOp
{
public:
    explicit EExprWinElemOp( A a, B b ): a_( a ), b_( b ) {}

    inline void eexpr_outstanding_gets() const
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: eexpr_outstanding_gets(): 264]"<<std::endl;
#endif
        a_.eexpr_outstanding_gets();
        b_.eexpr_outstanding_gets();
    }

    inline T eval() const { return OP::apply( a_.eval(), b_.eval() ); }

    inline bool is_win_b() const
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: is_win_b(): 275]"<<std::endl;
#endif
        if ( !a_.is_win_b() || !b_.is_win_b() )
            return false;

        return true;
    }

    // thwart compiler errors
    inline void eexpr_outstanding_put( const T val ) const {};
    inline WinCompletion completion() const { return INVALID_FLUSH; };
    inline void flush_win() const {};
    inline void flush_expr() const {}
private:
    A a_;
    B b_;
};

template <typename T, class A, class OP>
class EExprWinScalarOp
{
public:
    explicit EExprWinScalarOp( A a, T c ): a_( a ), c_( c ), c_left_( false ) {}
    explicit EExprWinScalarOp( T c, A a ): a_( a ), c_( c ), c_left_( true )  {}

    inline void eexpr_outstanding_gets() const
    { a_.eexpr_outstanding_gets(); }

    inline T eval() const
    {
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-bulk.hpp: eval: 306]"<<std::endl;
#endif
        if ( c_left_ ) return OP::apply( c_, a_.eval() );

        return OP::apply( a_.eval(), c_ );
    }

    inline bool is_win_b() const { return a_.is_win_b(); }

    // thwart compiler errors
    inline void eexpr_outstanding_put( const T val ) const {};
    inline WinCompletion completion() const { return INVALID_FLUSH; };
    inline void flush_win() const {};
    inline void flush_expr() const {}
private:
    A a_;
    T c_;
    bool c_left_;
};

// operators
// +
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Add<T>>>
operator+( EExpr<T,A> a, EExpr<T,B> b )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator +: 333]"<<std::endl;
#endif
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Add<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}
// *
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp<T, EExpr<T,A>, EExpr<T,B>, Mul<T>>>
operator *( EExpr<T,A> a, EExpr<T,B> b )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator *: 344]"<<std::endl;
#endif
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Mul<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}
// -
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp<T, EExpr<T,A>, EExpr<T,B>, Sub<T>>>
operator -( EExpr<T,A> a, EExpr<T,B> b )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator -: 355]"<<std::endl;
#endif
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Sub<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}
// /
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp<T, EExpr<T,A>, EExpr<T,B>, Div<T>>>
operator /( EExpr<T,A> a, EExpr<T,B> b )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator /: 366]"<<std::endl;
#endif
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Div<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}

// scalar (op) EExpr
// *
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Mul<T>>>
operator *( typename id<T>::type d, EExpr<T,A> a )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: (scalar) operator *: 379]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Mul<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}
// +
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Add<T>>>
operator +( typename id<T>::type d, EExpr<T,A> a )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: (scalar) operator +: 390]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Add<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}
// -
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Sub<T>>>
operator -( typename id<T>::type d, EExpr<T,A> a )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: (scalar) operator -: 401]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Sub<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}
// /
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Div<T>>>
operator /( typename id<T>::type d, EExpr<T,A> a )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: (scalar) operator /: 412]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Div<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}

// EExpr (op) scalar
// *
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Mul<T>>>
operator *( EExpr<T,A> a, typename id<T>::type d )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator (scalar) *: 425]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Mul<T>> ExprT;
    return EExpr<T, ExprT>( ExprT( a,d ) );
}
// +
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Add<T>>>
operator +( EExpr<T,A> a, typename id<T>::type d )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator (scalar) >>: 436]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Add<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,d ) );
}
// -
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Sub<T>>>
operator -( EExpr<T,A> a, typename id<T>::type d )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator (scalar) -: 447]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Sub<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,d ) );
}
// /
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Div<T>>>
operator /( EExpr<T,A> a, typename id<T>::type d )
{
#ifdef DEBUG
        std::cout<<"|DEBUG| [rmacxx-expr-elem.hpp: operator (scalar) /: 458]"<<std::endl;
#endif
    typedef EExprWinScalarOp <T, EExpr<T,A>, Div<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,d ) );
}
