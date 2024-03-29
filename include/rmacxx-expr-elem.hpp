// expression class that takes
// a reference
// required to wrap the window object
// to create an EExpr object

template <typename T, class WIN>
class RefEExpr
{
public:
    RefEExpr( WIN const& win ) : win_( win ) {}

   inline T eval() const { return win_.eval(); }
    inline bool is_win_b() const { return win_.is_win_b(); }
    inline WinCompletion completion() const { return win_.completion(); }
    inline void flush_win() const { win_.flush_win(); }
    inline void flush_expr() const { win_.flush_expr(); }
#ifndef RMACXX_USE_CLASSIC_HANDLES
    inline void block_on_expr(exprid expr) const { win_.block_on_expr(expr); }
#endif
    inline void eexpr_outstanding_gets() const
    { win_.eexpr_outstanding_gets(); }
    inline void eexpr_outstanding_put( const T val ) const
    { win_.eexpr_outstanding_put( val ); }
    inline void expr_ignore_last_get() const
    { win_.expr_ignore_last_get(); }
#ifdef RMACXX_DEBUG_EXPRS
    inline void debug()const{
        std::cout<<"@"<<(u_long)&win_;
    };
#endif
private:
    WIN const& win_;
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
#ifndef RMACXX_USE_CLASSIC_HANDLES
    inline void block_on_expr(exprid expr) const { v_.block_on_expr(expr); }
#endif
    inline void eexpr_outstanding_gets() const
    { v_.eexpr_outstanding_gets(); }
    inline void eexpr_outstanding_put( const T val ) const
    { v_.eexpr_outstanding_put( val ); }
    inline void expr_ignore_last_get() const
    { v_.expr_ignore_last_get(); }
#ifdef RMACXX_USE_CLASSIC_HANDLES
    inline static void flush()
    {
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
#endif
    // >> triggers the evaluation
    inline void operator >>( T& d )
    {
#ifdef RMACXX_DEBUG_EXPRS
        this->debug();
#endif
        // post gets
        eexpr_outstanding_gets();

        if ( is_win_b() ){
            d = eval();
        }
        else
        {
#if defined(RMACXX_USE_CLASSIC_HANDLES)
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
#else
            exprid id = FuturesManager<T>::instance().new_expr(
                std::async(std::launch::deferred, [&d,*this] () mutable {
                    d = this->eval();
                    std::cout << "Value of d: " << d << std::endl;
                }));
            this->block_on_expr(id);
#endif
        }
    }

    template <class W>
    void operator >>( EExpr<T,W> const& win )
    {
        bool is_placed = true;
        printf("AAAA\n");

        // ignore previous get
        win.expr_ignore_last_get();
        printf("BBBB\n");

        // post remaining gets for current
        // object
        eexpr_outstanding_gets();

        #if defined(RMACXX_USE_CLASSIC_HANDLES)
                T* c = static_cast<T*>( Handles<T>::instance().get_eexpr_ptr( sizeof( T ) ) );
        #else
                T* c =FuturesManager<T>::instance().allocate(1);
        #endif
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
        {
            // *c = eval();
            win.eexpr_outstanding_put(eval());
        }
        else
        {
#if defined(RMACXX_USE_CLASSIC_HANDLES)
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
#else
            exprid id = FuturesManager<T>::instance().new_expr(
                std::async(std::launch::deferred,[win,*this,c]{
                    *c = this->eval();
 #ifdef RMACXX_DEBUG_EXPRS
                    std::cout<< "Evaluated to "<< *c <<std::endl;
 #endif                   
                    win.eexpr_outstanding_put(*c);
                }));
            this->block_on_expr(id);
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
#if defined(RMACXX_USE_CLASSIC_HANDLES)
#if defined(RMACXX_EEXPR_USE_PLACEMENT_NEW_ALWAYS)
                Handles<T>::instance().eexpr_handles_.emplace_back( c );
#else
                Handles<T>::instance().eexpr_handles_.emplace_back( c, is_placed );

#endif
#else
                //TODO: Futures for local flush
#endif
            }
#if defined(RMACXX_USE_CLASSIC_HANDLES)
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
#else
            //TODO: Futures for manual flush
#endif
        }
    }

#ifdef RMACXX_DEBUG_EXPRS
    inline void debug()const{
        std::cout<<"<E>(";
        v_.debug();
        std::cout<<")";
    };
#endif
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
        a_.eexpr_outstanding_gets();
        b_.eexpr_outstanding_gets();
    }

#ifndef RMACXX_USE_CLASSIC_HANDLES
    inline void block_on_expr(exprid expr) const {
        a_.block_on_expr(expr);
        b_.block_on_expr(expr);
    }
#endif
   inline T eval() const { return OP::apply( a_.eval(), b_.eval() ); }

    inline bool is_win_b() const
    {
        if ( !a_.is_win_b() || !b_.is_win_b() )
            return false;

        return true;
    }

    // thwart compiler errors
    inline void eexpr_outstanding_put( const T val ) const {};
    inline WinCompletion completion() const { return INVALID_FLUSH; };
    inline void flush_win() const {};
    inline void flush_expr() const {}
#ifdef RMACXX_DEBUG_EXPRS
    inline void debug()const {
        std::cout<<"(";
        a_.debug();
        std::cout<<OP::S;
        b_.debug();
        std::cout<<")";
    };
#endif
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
#ifndef RMACXX_USE_CLASSIC_HANDLES
    inline void block_on_expr(exprid expr) const { a_.block_on_expr(expr); }
#endif
   inline T eval() const
   {
       if ( c_left_ ) return OP::apply( c_, a_.eval() );
       return OP::apply( a_.eval(), c_ );
   }
    inline bool is_win_b() const { return a_.is_win_b(); }

    // thwart compiler errors
    inline void eexpr_outstanding_put( const T val ) const {};
    inline WinCompletion completion() const { return INVALID_FLUSH; };
    inline void flush_win() const {};
    inline void flush_expr() const {}
#ifdef RMACXX_DEBUG_EXPRS
    inline void debug()const{
        if (c_left_){
            std::cout<<"(T";
            std::cout<<OP::S;
            a_.debug();
            std::cout<<")";
        }else{
            std::cout<<"(";
            a_.debug();
            std::cout<<OP::S;
            std::cout<<"T)";
        }
    };
#endif
private:
    A a_;
    T c_;
    bool c_left_;
};

// operators
// +
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Add<T> > >
operator+( EExpr<T,A> a, EExpr<T,B> b )
{
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Add<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}
// *
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp<T, EExpr<T,A>, EExpr<T,B>, Mul<T> > >
operator *( EExpr<T,A> a, EExpr<T,B> b )
{
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Mul<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}
// -
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp<T, EExpr<T,A>, EExpr<T,B>, Sub<T> > >
operator -( EExpr<T,A> a, EExpr<T,B> b )
{
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Sub<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}
// /
template <typename T, class A, class B>
EExpr <T, EExprWinElemOp<T, EExpr<T,A>, EExpr<T,B>, Div<T> > >
operator /( EExpr<T,A> a, EExpr<T,B> b )
{
    typedef EExprWinElemOp <T, EExpr<T,A>, EExpr<T,B>, Div<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,b ) );
}

// scalar (op) EExpr
// *
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Mul<T> > >
operator *( typename id<T>::type d, EExpr<T,A> a )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Mul<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}
// +
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Add<T> > >
operator +( typename id<T>::type d, EExpr<T,A> a )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Add<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}
// -
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Sub<T> > >
operator -( typename id<T>::type d, EExpr<T,A> a )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Sub<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}
// /
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Div<T> > >
operator /( typename id<T>::type d, EExpr<T,A> a )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Div<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( d,a ) );
}

// EExpr (op) scalar
// *
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Mul<T> > >
operator *( EExpr<T,A> a, typename id<T>::type d )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Mul<T>> ExprT;
    return EExpr<T, ExprT>( ExprT( a,d ) );
}
// +
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Add<T> > >
operator +( EExpr<T,A> a, typename id<T>::type d )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Add<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,d ) );
}
// -
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Sub<T> > >
operator -( EExpr<T,A> a, typename id<T>::type d )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Sub<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,d ) );
}
// /
template <typename T, class A>
EExpr <T, EExprWinScalarOp<T, EExpr<T,A>, Div<T> > >
operator /( EExpr<T,A> a, typename id<T>::type d )
{
    typedef EExprWinScalarOp <T, EExpr<T,A>, Div<T>> ExprT;
    return EExpr <T, ExprT>( ExprT( a,d ) );
}
