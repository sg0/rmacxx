// Local view window (NVSHMEM backend)
// -----------------------------------
// Accesses are expressed as (target PE, coordinates in the target's local
// n-d array). All PEs are assumed to use the same local extents.
//
// TODO FIXME finish coding RMACXX_USE_MPI_AINT_FOR_COORD.
// This is for cases when input coordinates are greater
// than INT range.

// specialized template for local view
template <typename T,
          WinUsage wuse_,
          WinCompletion wcmpl_,
          WinAtomicity watmc_,
          WinThreadSafety wtsft_>
class Window<T, LOCAL_VIEW, wuse_, wcmpl_, watmc_, wtsft_>
{
    using WIN = Window <T, LOCAL_VIEW, wuse_, wcmpl_, watmc_, wtsft_>;

#ifdef RMACXX_USE_CLASSIC_HANDLES
    #define LWFLUSH() do {\
        if ( is_expr_elem_ ) EExpr<T, WIN>::flush();\
        if ( is_expr_bulk_ ) BExpr<T, WIN>::flush();\
        flush_expr();\
    } while(0)
#else
    #define LWFLUSH() do{ \
        for(auto expr:expressions_){ \
            FuturesManager<T>::instance().unblock_expr(expr);\
        }                 \
        flush_expr();\
    } while(0)
#endif

public:
    /* Win_allocate model */
    Window( std::vector<int> const& dims ) { init_( dims, nullptr ); }

    /* Win_create model: the window is initialized from the user's buffer
     * and the window contents are copied back into it by wfree(). NVSHMEM
     * cannot make arbitrary user memory remotely accessible, so in between
     * use wget() (device pointer) to access the local part. */
    Window( std::vector<int> const& dims, T* winbuf ) { init_( dims, winbuf ); }

#if RMACXX_USE_MPI_BOOTSTRAP
    // communicator must be congruent with MPI_COMM_WORLD
    Window( std::vector<int> const& dims, MPI_Comm comm )
    {
        detail::check_world_comm( comm );
        init_( dims, nullptr );
    }

    Window( std::vector<int> const& dims, MPI_Comm comm, T* winbuf )
    {
        detail::check_world_comm( comm );
        init_( dims, winbuf );
    }
#endif

    // user has to call wfree to deallocate
    // resources (collective)
    ~Window()
    {
#ifndef RMACXX_USE_CLASSIC_HANDLES
        FuturesManager<T>::instance().forget_all( expressions_ );
#endif
    }

    void wfree()
    {
        if ( !eng_.created() )
            return;

        if ( wuse_ == EXPR )
        {
            xs_.clear( eng_ );
            LW_CLEAR_EXPR_();
        }

        eng_.complete();

        if ( user_buf_ != nullptr )
            eng_.d2h( user_buf_, eng_.base(), nelems_ * sizeof( T ) );

        eng_.destroy();
        iswinlocked_ = false;
        subsizes_.clear();
        starts_.clear();
        dims_.clear();
        expr_bptr_ = nullptr;
    }

    // Window attributes
#if RMACXX_USE_MPI_BOOTSTRAP
    inline MPI_Comm comm() { return MPI_COMM_WORLD; }
#endif
    inline nvshmem_team_t team() const { return NVSHMEM_TEAM_WORLD; }
    inline int ndims() const { return ndims_; }
    inline const std::vector<int>& dims() const { return dims_; }
    inline void barrier() { eng_.barrier(); }
    inline int size() const { return commSize_; }
    inline int rank() const { return commRank_; }
    inline WinCompletion completion() const { return wcmpl_; }
    // is my window locked?
    inline bool iswinlocked() const { return iswinlocked_; }
    // stream used by host-initiated operations of this window
    inline cudaStream_t stream() const { return eng_.stream(); }

    // view for device-initiated communication (pass by value to kernels)
    DeviceWindow<T, LOCAL_VIEW> device_view() const
    {
        DeviceWindow<T, LOCAL_VIEW> dv;
        dv.base = eng_.base();
        dv.ndims = ndims_;

        for ( int k = 0; k < RMACXX_MAX_DIMS; k++ )
            dv.dims[k] = ( k < ndims_ ) ? dims_[k] : 1;

        dv.nelems = nelems_;
        dv.me = commRank_;
        dv.npes = commSize_;
        return dv;
    }

    // return a pointer to the window buffer
    // NOTE: this is GPU (symmetric heap) memory
    inline T* wget() const { return eng_.base(); }

    // fill value in the buffer attached to the window
    // (collective)
    inline void fill( T val ) const
    {
        eng_.complete();
        std::vector<T> h( static_cast<size_t>( nelems_ ), val );
        eng_.h2d( eng_.base(), h.data(), nelems_ * sizeof( T ) );
        eng_.barrier();
    }

    // returns 2d if ndims == 2,
    // otherwise returns data as per flat 1d layout
    void print( std::string const& s ) const
    {
        eng_.barrier();
        std::vector<T> buf( static_cast<size_t>( nelems_ ) );
        eng_.d2h( buf.data(), eng_.base(), nelems_ * sizeof( T ) );

        if ( commRank_ == 0 )
            std::cout << s << std::endl;

        for ( int p = 0; p < commSize_; p++ )
        {
            nvshmem_barrier_all();

            if ( p == commRank_ )
            {
                if ( ndims_ == 2 ) // 2d layout
                {
                    const int m = dims_[0];
                    const int n = dims_[1];
                    std::cout << "[" << p << "]" << "dims: {" << m << "," << n << "}" << std::endl;

                    for ( int i = 0; i < m; i++ )
                    {
                        for ( int j = 0; j < n; j++ )
                            std::cout << " " << buf[i*n + j] << " ";

                        std::cout << std::endl;
                    }
                }
                else // 1d layout
                {
                    std::cout << "[" << p << "]" << "dims: {" << nelems_ << "}" << std::endl;

                    for ( int i = 0; i < nelems_; i++ )
                        std::cout << buf[i] << " ";

                    std::cout << std::endl;
                }

                std::cout << std::flush;
            }
        }

        nvshmem_barrier_all();
    }

    // there is no lock/unlock epoch in NVSHMEM; these are kept for
    // source compatibility and only track a flag
    inline void wunlock() { iswinlocked_ = false; }
    inline void wlock() { iswinlocked_ = true; }

    // check if window's WinCompletion is
    // default blocking
    inline bool is_win_b() const
    {
        if ( wcmpl_ == NO_FLUSH )
            return false;

        return true;
    }

    /* Overloaded () operators */
    /*-------------------------*/

    // choose the window object based on wuse_ in compile time
    inline typename std::conditional<( wuse_==NO_EXPR ), WIN&, EExpr<T,RefEExpr<T,WIN>>>::type
    operator()( int target, std::initializer_list<int> const& lo )
    { return operator()( target, lo, std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    inline typename std::conditional<( wuse_==NO_EXPR ), WIN&, BExpr<T,RefBExpr<T,WIN>>>::type
    operator()( int target, std::initializer_list<int> const& lo, std::initializer_list<int> const& hi )
    { return operator()( target, lo, hi, std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    // large offsets
    inline typename std::conditional<( wuse_==NO_EXPR ), WIN&, EExpr<T,RefEExpr<T,WIN>>>::type
    operator()( int target, std::initializer_list<int64_t> const& lo )
    { return operator()( target, lo, std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    inline typename std::conditional<( wuse_==NO_EXPR ), WIN&, BExpr<T,RefBExpr<T,WIN>>>::type
    operator()( int target, std::initializer_list<int64_t> const& lo, std::initializer_list<int64_t> const& hi )
    { return operator()( target, lo, hi, std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    // for put/get
    // --------------------
    // using X/Y tags to differentiate between
    // EXPR windows with standard ones
    template <typename I>
    inline WIN& operator()( int target, std::initializer_list<I> const& lo, std::initializer_list<I> const& hi, X )
    {
        lock();
        target_ = target;
        set_box_( lo, hi );
        return *this;
    }

    template <typename I>
    inline WIN& operator()( int target, std::initializer_list<I> const& lo, X )
    {
        lock();
        target_ = target;
        set_elem_( lo );
        return *this;
    }

    // Window object wrapped with an
    // (element-wise/bulk) expression
    // object
    EExpr<T,RefEExpr<T,WIN>> operator()( int target, std::initializer_list<int> const& lo, Y )
    {
        lock();
        is_expr_elem_ = true;
        target_ = target;
        set_elem_( lo );
        int idx = xs_.push( make_rec_() );
        unlock();
        return EExpr<T,RefEExpr<T,WIN>>( RefEExpr<T,WIN>( *this, idx ) );
    }

    EExpr<T,RefEExpr<T,WIN>> operator()( int target, std::initializer_list<int64_t> const& lo, Y )
    {
        lock();
        is_expr_elem_ = true;
        target_ = target;
        set_elem_( lo );
        int idx = xs_.push( make_rec_() );
        unlock();
        return EExpr<T,RefEExpr<T,WIN>>( RefEExpr<T,WIN>( *this, idx ) );
    }

    template <typename I>
    BExpr<T,RefBExpr<T,WIN>> operator()( int target, std::initializer_list<I> const& lo, std::initializer_list<I> const& hi, Y )
    {
        lock();
        is_expr_bulk_ = true;
        target_ = target;
        set_box_( lo, hi );
        int idx = xs_.push( make_rec_() );
        unlock();
        return BExpr<T,RefBExpr<T,WIN>>( RefBExpr<T,WIN>( *this, idx ) );
    }

    // post outstanding gets for a prior element-wise /
    // bulk expression window access
    void eexpr_outstanding_gets( int idx ) const
    {
        lock();
        xs_.issue_get( eng_, idx, watmc_ == ATOMIC_PUT_GET );
        unlock();
    }

    void bexpr_outstanding_gets( int idx ) const
    {
        lock();
        xs_.issue_get( eng_, idx, watmc_ == ATOMIC_PUT_GET );
        unlock();
    }

    // ignore get, since the communication
    // params are going to be used for a put
    inline void expr_ignore_last_get( int idx ) const
    {
        lock();
        xs_.mark_dest( idx );
        unlock();
    }

    // puts

    // this is required for EXPR cases when there is
    // a window on the RHS to >>
    // the origin is staged, so it is locally complete
    // on return
    void eexpr_outstanding_put( int idx, const T val ) const
    {
        lock();
        xs_.put( eng_, idx, &val, watmc_ == ATOMIC_PUT_GET );
        unlock();
    }

    void bexpr_outstanding_put( int idx, const T* origin_addr ) const
    {
        lock();
        xs_.put( eng_, idx, origin_addr, watmc_ == ATOMIC_PUT_GET );
        unlock();
    }

    // for acc
    // -------
    template <typename I>
    inline WIN& operator()( int target, std::initializer_list<I> const& lo, std::initializer_list<I> const& hi, Op op )
    {
        lock();
        target_ = target;
        winop_ = op;
        set_box_( lo, hi );
        return *this;
    }

    template <typename I>
    inline WIN& operator()( int target, std::initializer_list<I> const& lo, Op op )
    {
        lock();
        target_ = target;
        winop_ = op;
        set_elem_( lo );
        return *this;
    }

    // for get-accumulate
    // ------------------
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, std::initializer_list<int> const& hi, Op op, T const* inpval )
    {
        lock();
        target_ = target;
        winop_ = op;
        is_fop_ = true;
        fop_inp_ = inpval;
        set_box_( lo, hi );
        return *this;
    }

    // for fetch-and-op
    // ----------------
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, Op op, T inpval )
    {
        lock();
        target_ = target;
        winop_ = op;
        fop_val_ = inpval;
        fop_inp_ = &fop_val_;
        is_fop_ = true;
        set_elem_( lo );
        return *this;
    }

    // for compare-and-swap
    // --------------------
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, const T inpval, const T cmpval )
    {
        lock();
        target_ = target;
        fop_val_ = inpval;
        cas_val_ = cmpval;
        is_cas_ = true;
        set_elem_( lo );
        return *this;
    }

    // evaluate (complete) an element-wise expression access
    T eval( int idx ) const
    {
        lock();
        T val = xs_.eval( eng_, idx, watmc_ == ATOMIC_PUT_GET );
        unlock();
        return val;
    }

    // return the last count
    inline int get_count() const { return count_dimn_; }
    inline int get_count( int idx ) const { return static_cast<int>( xs_.count( idx ) ); }

    // bulk expression evaluation
    // returns the count of elements processed in get
    int fillInto( int idx, T* buf ) const
    {
        lock();
        int count = static_cast<int>( xs_.fill_into( eng_, idx, buf, watmc_ == ATOMIC_PUT_GET ) );
        expr_bptr_ = buf;
        unlock();
        return count;
    }

    // clear all the metadata corresponding
    // to expressions...this is called inside
    // flush that complete all prior expressions
    // to that point
    void flush_expr() const
    {
#ifndef RMACXX_USE_CLASSIC_HANDLES
        // deferred (futures) expressions that also depend on other, not
        // yet flushed windows still need this window's records
        for ( auto e : expressions_ )
            if ( FuturesManager<T>::instance().is_alive( e ) )
                return;
#endif
        if ( wuse_ == EXPR )
            xs_.clear( eng_ );

        LW_CLEAR_EXPR_();
        expr_bptr_   = nullptr;
        is_expr_elem_ = false;
        is_expr_bulk_ = false;
    }

    // operators overloading for standard
    // put/get
    // overload istream op for bulk `put/acc
    // -------------------------------------
    void operator <<( const T* origin_addr )
    {
        detail::HostLayout lay = packed_layout_();
        put_or_acc_( lay, origin_addr );
    }

    // overloading ostream op for bulk `get
    // ------------------------------------
    void operator >>( T* origin_addr )
    {
        detail::HostLayout lay = packed_layout_();
        get_or_gacc_( lay, origin_addr );
    }

    // operator overloading for put/get
    // with user-defined (strided) origin buffers
    // NC stands for non-contiguous
    // TODO FIXME although Subarray_t only makes sense
    // for bulk cases, if someone uses elementwise
    // interface on the LHS, the compiler will not flag
    // it as an error currently, fix that

    // overload istream op for bulk noncontiguous `put/acc
    // ---------------------------------------------------
    void operator <<( RMACXX_Subarray_t<T, LOCAL_VIEW>& origin )
    {
        detail::HostLayout lay = subarray_layout_( origin );
        put_or_acc_( lay, origin.ptr_ );
    }

    // overloading ostream op for bulk noncontiguous `get
    // --------------------------------------------------
    void operator >>( RMACXX_Subarray_t<T, LOCAL_VIEW>& origin )
    {
        detail::HostLayout lay = subarray_layout_( origin );
        get_or_gacc_( lay, origin.ptr_ );
    }

    // overload istream op for element-wise `put/acc
    // ---------------------------------------------
    void operator <<( const T val )
    {
        detail::HostLayout lay = detail::HostLayout::packed1( 1 );
        put_or_acc_( lay, &val );
    }

    // overloading ostream op for element-wise `get
    // OR fetch-and-op OR compare-and-swap
    // --------------------------------------------
    void operator >>( T& val )
    {
        detail::HostLayout lay = detail::HostLayout::packed1( 1 );

        if ( is_cas_ )
        {
            std::vector<detail::PatchDesc> pieces = pieces_();
            val = cas_val_; // compare value, replaced by the result
            eng_.issue( pieces, lay, detail::Xfer::CAS, OpKind::NONE, &val, &val, &fop_val_, true );
            is_cas_ = false;
            unlock();
        }
        else
            get_or_gacc_( lay, &val );
    }

    // synchronization
    // NVSHMEM has no per-target completion: these complete everything
    inline void flush_local( int const& target ) const { (void)target; eng_.complete_local(); }
    inline void flush( int const& target ) const { (void)target; eng_.complete(); }
    // make host-API updates of the local window visible
    inline void sync() const { RMACXX_CUDA_CHECK( cudaStreamSynchronize( eng_.stream() ) ); }

    inline void flush() const { flush( std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }
    inline void flush_local() const { flush_local( std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    inline void flush( X ) const { eng_.complete(); }
    inline void flush_local( X ) const { eng_.complete_local(); }

    // users would call flush when there is window
    // in the RHS, when the window is created with either
    // (LOCAL|NO)_FLUSH...
    inline void flush( Y ) const
    {
        // cases with LOCAL_FLUSH and buffer in RHS will
        // encounter some extra work, but the user must not
        // have invoked flush in that case anyway
        if ( wcmpl_ == NO_FLUSH || wcmpl_ == LOCAL_FLUSH )
        {
            LWFLUSH();
        }

        eng_.complete();
    }

#ifndef RMACXX_USE_CLASSIC_HANDLES
    inline void block_on_expr(exprid expr) const {
        this->expressions_.emplace_back(expr);
        FuturesManager<T>::instance().block_expr(expr);
    }
#endif

    // this is to make it easy to just call flush_all
    // from the expression classes
    inline void flush_win() const { eng_.complete(); }
    inline void flush_win_local() const { eng_.complete_local(); }

    inline void flush_local( Y ) const
    {
        if ( wcmpl_ == NO_FLUSH )
        {
            LWFLUSH();
        }

        eng_.complete_local();
    }

    // thwart attempts to copy objects
    // using standard assignment and parenthesis
    Window( const Window& ) = delete;
    Window& operator=( const Window& other ) = delete;

private:
    void init_( std::vector<int> const& dims, T* winbuf )
    {
        detail::ensure_init();

        if ( dims.empty() || dims.size() > RMACXX_MAX_DIMS )
            RMACXX_FATAL( "window dimensionality must be in [1, RMACXX_MAX_DIMS]" );

        ndims_ = static_cast<int>( dims.size() );
        dims_ = dims;
        nelems_ = 1;

        for ( int d : dims_ )
            nelems_ *= d;

        eng_.create( nelems_ );
        commRank_ = eng_.me();
        commSize_ = eng_.npes();
        user_buf_ = winbuf;

        if ( winbuf != nullptr )
            eng_.h2d( eng_.base(), winbuf, nelems_ * sizeof( T ) );

        target_ = -1;
        disp_ = 0;
        count_dimn_ = 0;
        elem_access_ = true;
        fop_inp_ = nullptr;
        is_fop_ = false;
        is_cas_ = false;
        winop_ = Op();
        is_expr_elem_ = false;
        is_expr_bulk_ = false;
        expr_bptr_ = nullptr;

        if ( wuse_ == EXPR )
        {
            xs_.init( eng_ );
            LW_RESERVE_EXPR_();
        }

        subsizes_.resize( ndims_ );
        starts_.resize( ndims_ );
        iswinlocked_ = true;
        eng_.barrier();
    }

#ifdef RMACXX_USE_CLASSIC_HANDLES
    void LW_RESERVE_EXPR_() const {}
    void LW_CLEAR_EXPR_() const {}
#else
    void LW_RESERVE_EXPR_() const { expressions_.reserve( DEFAULT_EXPR_COUNT ); }
    void LW_CLEAR_EXPR_() const { expressions_.clear(); }
#endif

    // element access: linear displacement in the target's array
    template <typename I>
    void set_elem_( std::initializer_list<I> const& lo )
    {
        elem_access_ = true;
        count_dimn_ = 1;

        if ( ndims_ > 1 && lo.size() > 1 )
        {
            disp_ = 0;
            int idx = 0;

            for ( I val : lo )
            {
                int64_t v = val;
                for ( int i = ndims_-1; i >= idx+1; i-- ) v *= dims_[i];
                disp_ += v;
                idx++;
            }
        }
        else
            disp_ = *( lo.begin() );
    }

    // bulk access: box [lo, hi]
    template <typename I>
    void set_box_( std::initializer_list<I> const& lo, std::initializer_list<I> const& hi )
    {
        elem_access_ = false;

        if ( ndims_ > 1 )
        {
            if ( lo.size() != static_cast<size_t>( ndims_ ) || hi.size() != static_cast<size_t>( ndims_ ) )
                RMACXX_FATAL( "number of coordinates does not match window dimensionality" );

            int i = 0;
            count_dimn_ = 1;
            auto ilo = lo.begin();

            for ( I hi_val : hi )
            {
                subsizes_[i] = static_cast<int>( hi_val - *ilo + 1 );
                starts_[i] = static_cast<int>( *ilo );
                count_dimn_ *= subsizes_[i];
                i++;
                ilo++;
            }

            disp_ = 0;
        }
        else
        {
            count_dimn_ = static_cast<int>( *( hi.begin() ) - *( lo.begin() ) + 1 );
            disp_ = *( lo.begin() );
        }
    }

    // descriptor for the current access
    std::vector<detail::PatchDesc> pieces_() const
    {
        std::vector<detail::PatchDesc> v( 1 );

        if ( ndims_ > 1 && !elem_access_ )
            v[0] = detail::make_box_desc( target_, ndims_, dims_.data(), starts_.data(), subsizes_.data() );
        else
            v[0] = detail::make_linear_desc( target_, disp_, elem_access_ ? 1 : count_dimn_, nelems_ );

        return v;
    }

    detail::HostLayout packed_layout_() const
    {
        if ( ndims_ > 1 && !elem_access_ )
            return detail::HostLayout::packed( ndims_, subsizes_.data() );

        return detail::HostLayout::packed1( elem_access_ ? 1 : count_dimn_ );
    }

    detail::HostLayout subarray_layout_( RMACXX_Subarray_t<T, LOCAL_VIEW> const& origin ) const
    {
        detail::HostLayout lay = packed_layout_();

        if ( lay.nd > 1 )
        {
            if ( origin.sizes_.size() != static_cast<size_t>( lay.nd ) || origin.starts_.size() != static_cast<size_t>( lay.nd ) )
                RMACXX_FATAL( "RMACXX_Subarray_t: starts/sizes must match window dimensionality" );

            for ( int k = 0; k < lay.nd; k++ )
            {
                lay.odims[k] = origin.sizes_[k];
                lay.ostart[k] = origin.starts_[k];
            }
        }
        else
        {
            // 1D: the patch starts at starts_[0] of the origin buffer; as in
            // the MPI version, sizes_ is not used to bound the buffer
            lay.ostart[0] = origin.starts_.empty() ? 0 : origin.starts_[0];
            lay.odims[0] = lay.ostart[0] + lay.ext[0];
        }

        return lay;
    }

    detail::ExprRec make_rec_() const
    {
        detail::ExprRec r;
        r.pieces = pieces_();
        r.lay = packed_layout_();
        r.count = r.lay.count();
        return r;
    }

    void finish_op_()
    {
        if ( wcmpl_ == REMOTE_FLUSH )
            eng_.complete();
        else if ( wcmpl_ == LOCAL_FLUSH )
            eng_.complete_local();
    }

    void put_or_acc_( detail::HostLayout const& lay, const T* origin )
    {
        std::vector<detail::PatchDesc> pieces = pieces_();

        if ( winop_.op == OpKind::NONE )
            eng_.issue( pieces, lay, ( watmc_ == ATOMIC_PUT_GET ) ? detail::Xfer::APUT : detail::Xfer::PUT,
                        OpKind::NONE, origin, nullptr, nullptr, false );
        else
        {
            eng_.issue( pieces, lay, detail::Xfer::ACC, winop_.op, origin, nullptr, nullptr, false );
            winop_ = Op();
        }

        finish_op_();
        unlock();
    }

    void get_or_gacc_( detail::HostLayout const& lay, T* origin )
    {
        std::vector<detail::PatchDesc> pieces = pieces_();

        if ( is_fop_ )
        {
            // the operand buffer is always contiguous (as in MPI_Get_accumulate
            // with a count of basic elements), the result may be strided
            detail::HostLayout oplay = packed_layout_();
            eng_.issue( pieces, lay, detail::Xfer::GACC, winop_.op, nullptr, origin, fop_inp_, false, &oplay );
            is_fop_ = false;
            winop_ = Op();
        }
        else
            eng_.issue( pieces, lay, ( watmc_ == ATOMIC_PUT_GET ) ? detail::Xfer::AGET : detail::Xfer::GET,
                        OpKind::NONE, nullptr, origin, nullptr, false );

        finish_op_();
        unlock();
    }

    // ------------------- //

    mutable detail::Engine<T> eng_;
    mutable detail::ExprState<T> xs_;

    int commRank_ = 0;
    int commSize_ = 0;

    bool iswinlocked_ = false;
    Op winop_;
    T* user_buf_ = nullptr;

    int ndims_ = 0;
    std::vector<int> dims_;
    int64_t nelems_ = 0;

    mutable int target_;
    // target displacement for element-wise
    // operations, calculated directly
    // from operator()
    mutable int64_t disp_;
    mutable bool elem_access_;

    // values/pointers associated with CAS/FOP
    const T* fop_inp_;
    T fop_val_{};
    T cas_val_{};
    bool is_fop_, is_cas_;

#ifndef RMACXX_USE_CLASSIC_HANDLES
    mutable std::vector<exprid> expressions_;
#endif
    mutable std::vector<int> subsizes_;
    mutable std::vector<int> starts_;
    mutable int count_dimn_;

    mutable bool is_expr_elem_, is_expr_bulk_;
    mutable T* expr_bptr_;

    // locks for protecting multithreaded
    // accesses to window
#ifdef RMACXX_USE_SPINLOCK
    void lock() const
    {
        if ( wtsft_ == CONCURRENT )
        {
            while ( locked.test_and_set( std::memory_order_acquire ) )
                ;
        }
    }
    void unlock() const
    {
        if ( wtsft_ == CONCURRENT )
            locked.clear( std::memory_order_release );
    }
#else
    // mutex to lock window
    mutable std::recursive_mutex win_mtx_;
    void lock() const { if ( wtsft_ == CONCURRENT ) win_mtx_.lock(); }
    void unlock() const { if ( wtsft_ == CONCURRENT ) win_mtx_.unlock(); }
#endif
}; // class Window<T,LOCAL_VIEW,...>
