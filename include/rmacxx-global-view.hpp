// Global view window (NVSHMEM backend)
// ------------------------------------
// Every PE contributes an n-d block [lo, hi] (inclusive, irregular sizes
// allowed) of a global array. Accesses use global arithmetic coordinates;
// a patch is split into one piece per owning PE by intersecting it with
// every PE's block.
//
// TODO FIXME finish coding RMACXX_USE_MPI_AINT_FOR_COORD.
// This is for cases when input coordinates are greater
// than INT range (count is still INT).

// specialized template for global view
template <typename T,
          WinUsage wuse_,
          WinCompletion wcmpl_,
          WinAtomicity watmc_,
          WinThreadSafety wtsft_>
class Window<T, GLOBAL_VIEW, wuse_, wcmpl_, watmc_, wtsft_>
{
    using WIN = Window <T, GLOBAL_VIEW, wuse_, wcmpl_, watmc_, wtsft_>;
#ifdef RMACXX_USE_CLASSIC_HANDLES
    #define GWFLUSH() do {\
        if ( is_expr_elem_ ) EExpr<T, WIN>::flush();\
        if ( is_expr_bulk_ ) BExpr<T, WIN>::flush();\
        flush_expr();\
    } while(0)
#else
    //TODO: FIX AND REIMPLEMENT FLUSH
    #define GWFLUSH() do{ \
        for(auto expr:expressions_){ \
            FuturesManager<T>::instance().unblock_expr(expr);\
        }                 \
        flush_expr();\
    } while(0)
#endif

public:
    // stores all the lo's and hi's, packaged together for each process
    std::vector<std::vector<int>> los;
    std::vector<std::vector<int>> his;

    Window( std::vector<int> const& lo, std::vector<int> const& hi ) { init_( lo, hi, nullptr ); }

    // Win_create model: the window is initialized from the user's buffer
    // and copied back into it by wfree() (see the local view)
    Window( std::vector<int> const& lo, std::vector<int> const& hi, T* winbuf ) { init_( lo, hi, winbuf ); }

#if RMACXX_USE_MPI_BOOTSTRAP
    // communicator must be congruent with MPI_COMM_WORLD
    Window( std::vector<int> const& lo, std::vector<int> const& hi, MPI_Comm comm )
    {
        detail::check_world_comm( comm );
        init_( lo, hi, nullptr );
    }

    Window( std::vector<int> const& lo, std::vector<int> const& hi, MPI_Comm comm, T* winbuf )
    {
        detail::check_world_comm( comm );
        init_( lo, hi, winbuf );
    }
#endif

    // user has to call wfree to deallocate
    // resources
    ~Window()
    {
#ifndef RMACXX_USE_CLASSIC_HANDLES
        FuturesManager<T>::instance().forget_all(expressions_);
#endif
    }

    void wfree()
    {
        if ( !eng_.created() )
            return;

        if ( wuse_ == EXPR )
        {
            xs_.clear( eng_ );
            GW_CLEAR_EXPR_();
        }

        eng_.complete();

        if ( user_buf_ != nullptr )
            eng_.d2h( user_buf_, eng_.base(), nelems_ * sizeof( T ) );

        eng_.destroy();

        if ( d_rlo_ ) { cudaFree( d_rlo_ ); d_rlo_ = nullptr; }
        if ( d_rhi_ ) { cudaFree( d_rhi_ ); d_rhi_ = nullptr; }

        iswinlocked_ = false;
        subsizes_.clear();
        starts_.clear();
        lsizes_.clear();
        lstarts_.clear();
        dims_.clear();
        dims_per_pe_.clear();
        pgrid_.clear();
        lo_.clear();
        hi_.clear();
        pe_coords_.clear();
        rlo_.clear();
        rhi_.clear();
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
    // process grid (number of distinct block starts per dimension)
    inline const std::vector<int>& pgrid() const { return pgrid_; }

    // view for device-initiated communication (pass by value to kernels)
    DeviceWindow<T, GLOBAL_VIEW> device_view() const
    {
        DeviceWindow<T, GLOBAL_VIEW> dv;
        dv.base = eng_.base();
        dv.ndims = ndims_;
        dv.me = commRank_;
        dv.npes = commSize_;
        dv.rlo = d_rlo_;
        dv.rhi = d_rhi_;
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

    // some functions for Global Arrays compatibility

    // TODO FIXME this will be purged in the future
    // NGA_Access (returns a *device* pointer)
    void access( int lo[], int hi[], T** ptr, int ld[] )
    {
        (void)hi;
        T* base = wget();

        if ( ndims_ > 1 )
        {
            int64_t idx = 0;

            for ( int k = 0; k < ndims_; k++ )
            {
                idx = idx * dims_[k] + ( lo[k] - rlo_[commRank_*ndims_+k] );

                // ld
                if ( k >= 1 )
                    ld[k-1] = dims_[k];
            }

            *ptr = &base[idx];
        }
        else
        {
            *ptr = &base[*lo - rlo_[commRank_]];
            *ld = 1;
        }
    }

    // this is just a test, and this is *incorrect*
    // correct symm = 0.5*(A + A')
    void symmetrize()
    {
        eng_.complete();
        eng_.scale_local( static_cast<T>( 2.0 ) );
        eng_.barrier();
    }

    // copy window lo/hi ranges
    void wdim( std::vector<int>& lo, std::vector<int>& hi ) const
    {
        lo.assign( rlo_.begin() + commRank_*ndims_, rlo_.begin() + ( commRank_+1 )*ndims_ );
        hi.assign( rhi_.begin() + commRank_*ndims_, rhi_.begin() + ( commRank_+1 )*ndims_ );
    }

    // copy window lo/hi ranges (1D)
    void wdim( int& lo, int& hi ) const
    {
        lo = rlo_[commRank_];
        hi = rhi_[commRank_];
    }

    // print data ranges per process
    void print_ranges() const
    {
        if ( commRank_ == 0 )
        {
            // PE grid info
            std::cout << "Process grid: " << std::endl;
            const int pdims = static_cast<int>( pgrid_.size() );

            for ( int i = 0; i < pdims; i++ )
            {
                if ( i == ( pdims-1 ) )
                    std::cout << pgrid_[i] << std::endl;
                else
                    std::cout << pgrid_[i] << " x ";
            }

            std::cout << "---x---" << std::endl;

            // PE data ranges
            for ( int p = 0; p < commSize_; p++ )
            {
                std::cout << "Process " << p << ": " << std::endl;
                std::cout << "Lo: ";

                for ( int i = 0; i < ndims_; i++ )
                {
                    std::cout << rlo_[p*ndims_+i];

                    if ( i != ( ndims_ - 1 ) )
                        std::cout << ", ";
                }

                std::cout << std::endl;
                std::cout << "Hi: ";

                for ( int i = 0; i < ndims_; i++ )
                {
                    std::cout << rhi_[p*ndims_+i];

                    if ( i != ( ndims_ - 1 ) )
                        std::cout << ", ";
                }
                std::cout << std::endl;
            }

            std::cout << std::endl;
            std::cout << "---x---" << std::endl;
        }

        nvshmem_barrier_all();
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
    // default (remote | local) blocking
    inline bool is_win_b() const
    {
        if ( wcmpl_ == NO_FLUSH )
            return false;

        return true;
    }

    // find remote target, disp from global input coordinate
    // (for elementwise)
    // -----------------------------------------------------
    inline void find_target_disp( std::initializer_list<int> const& l )
    {
        target_ = -1;
        disp_ = 0;

        if ( static_cast<int>( l.size() ) != ndims_ )
            RMACXX_FATAL( "number of coordinates does not match window dimensionality" );

        std::vector<int> c( l.begin(), l.end() );
        const int p = find_target( c );

        if ( p < 0 )
            RMACXX_FATAL( "coordinate is not owned by any process" );

        target_ = p;
        disp_ = 0;

        for ( int k = 0; k < ndims_; k++ )
            disp_ = disp_ * dims_per_pe_[p*ndims_+k] + ( c[k] - rlo_[p*ndims_+k] );
    }

    // returns target process owning an input coordinate
    inline int find_target( std::vector<int> const& l ) const
    {
        for ( int p = 0; p < commSize_; p++ )
        {
            bool found = true;

            for ( int k = 0; k < ndims_ && found; k++ )
                found = ( l[k] >= rlo_[p*ndims_+k] && l[k] <= rhi_[p*ndims_+k] );

            if ( found )
                return p;
        }

        return -1;
    }

    // returns target process owning an input coordinate (1D)
    // and updates the remote displacement
    inline int find_target( int const& lo )
    {
        for ( int p = 0; p < commSize_; p++ )
        {
            if ( ( lo >= rlo_[p] ) && ( lo <= rhi_[p] ) )
            {
                disp_ = lo - rlo_[p];
                return p;
            }
        }

        return -1;
    }

    /* Overloaded () operators */
    /*-------------------------*/

    // choose the window object based on wuse_ in compile time
    inline typename std::conditional<( wuse_==NO_EXPR ), WIN&, EExpr<T,RefEExpr<T,WIN>>>::type
    operator()( std::initializer_list<int> const& l )
    { return operator()( l, std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    inline typename std::conditional<( wuse_==NO_EXPR ), WIN&, BExpr<T,RefBExpr<T,WIN>>>::type
    operator()( std::initializer_list<int> const& l, std::initializer_list<int> const& h )
    { return operator()( l, h, std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    // for put/get in standard interface
    // ---------------------------------
    inline WIN& operator()( std::initializer_list<int> const& l,  std::initializer_list<int> const& h, X )
    {
#ifdef DEBUG_CHECK_LIMITS
        store_lo = l;
        store_hi = h;
#endif
        lock();
        set_box_( l, h );
        return *this;
    }

    // elementwise, we calculate target, disp
    // in this function
    inline WIN& operator()( std::initializer_list<int> const& l, X )
    {
        lock();
        elem_access_ = true;
        find_target_disp( l );
        return *this;
    }

    // Window object wrapped with an
    // (element-wise/bulk) expression
    // object
    inline EExpr<T,RefEExpr<T,WIN>> operator()( std::initializer_list<int> const& l, Y )
    {
        lock();
        is_expr_elem_ = true;
        elem_access_ = true;
        find_target_disp( l );
        count_dimn_ = 1;
        int idx = xs_.push( make_rec_() );
        unlock();
        return EExpr<T,RefEExpr<T,WIN>>( RefEExpr<T,WIN>( *this, idx ) );
    }

    // bulk expression
    inline BExpr<T,RefBExpr<T,WIN>> operator()( std::initializer_list<int> const& l, std::initializer_list<int> const& h, Y )
    {
        lock();
        is_expr_bulk_ = true;
        set_box_( l, h );
        int idx = xs_.push( make_rec_() );
        unlock();
        return BExpr<T,RefBExpr<T,WIN>>( RefBExpr<T,WIN>( *this, idx ) );
    }

    // post outstanding gets for a prior element-wise /
    // bulk expression window access
    inline void eexpr_outstanding_gets( int idx ) const
    {
        lock();
        xs_.issue_get( eng_, idx, watmc_ == ATOMIC_PUT_GET );
        unlock();
    }

    inline void bexpr_outstanding_gets( int idx ) const
    {
        lock();
        xs_.issue_get( eng_, idx, watmc_ == ATOMIC_PUT_GET );
        unlock();
    }

    // ignore get, since the communication
    // params are going to be used for put(s)
    inline void expr_ignore_last_get( int idx ) const
    {
        lock();
        xs_.mark_dest( idx );
        unlock();
    }

    // puts (window on the RHS of >>); origin is staged, so it is
    // locally complete on return
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
    inline WIN& operator()( std::initializer_list<int> const& l, std::initializer_list<int> const& h, Op op )
    {
        lock();
        winop_ = op;
        set_box_( l, h );
        return *this;
    }

    inline WIN& operator()( std::initializer_list<int> const& l, Op op )
    {
        lock();
        winop_ = op;
        elem_access_ = true;
        find_target_disp( l );
        return *this;
    }

    // for get-accumulate
    // ------------------
    inline WIN& operator()( std::initializer_list<int> const& l, std::initializer_list<int> const& h, Op op, T const* inpval )
    {
        lock();
        winop_ = op;
        is_fop_ = true;
        fop_inp_ = inpval;
        set_box_( l, h );
        return *this;
    }

    // for fetch-and-op
    // ----------------
    inline WIN& operator()( std::initializer_list<int> const& l, Op op, T inpval )
    {
        lock();
        winop_ = op;
        fop_val_ = inpval;
        fop_inp_ = &fop_val_;
        is_fop_ = true;
        elem_access_ = true;
        find_target_disp( l );
        return *this;
    }

    // for compare-and-swap
    // --------------------
    inline WIN& operator()( std::initializer_list<int> const& l, const T inpval, const T cmpval )
    {
        lock();
        is_cas_ = true;
        fop_val_ = inpval;
        cas_val_ = cmpval;
        elem_access_ = true;
        find_target_disp( l );
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

    // return the last element count for a window object
    // this is used to allocate memory for intermediate put
    // origin buffer in the bulk expression case with window
    // in the RHS
    inline int get_count() const { return count_dimn_; }
    inline int get_count( int idx ) const { return static_cast<int>( xs_.count( idx ) ); }

    // fillInto is used by bulk expression interface for
    // completing the gets of a window access (prefetched
    // into a device buffer, or fetched now); the patch is
    // returned packed in C order
    int fillInto( int idx, T* buf ) const
    {
        lock();
        int count = static_cast<int>( xs_.fill_into( eng_, idx, buf, watmc_ == ATOMIC_PUT_GET ) );
        expr_bptr_ = buf;
        unlock();
        return count;
    }

    // for bulk expression
    inline T operator()( int idx ) const { return expr_bptr_[idx]; }
    inline T& operator()( int idx ) { return expr_bptr_[idx]; }

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

        GW_CLEAR_EXPR_();
        expr_bptr_    = nullptr;
        is_expr_elem_ = false;
        is_expr_bulk_ = false;
    }

    /* Bulk operations */
    /* --------------- */

    // operators overloading for standard
    // bulk put/get
    // -------------------------------------
    void operator <<( const T* origin_addr )
    {
        put_or_acc_( packed_layout_(), origin_addr );
    }

    // overloading ostream op for bulk `get
    // ------------------------------------
    void operator >>( T* origin_addr )
    {
        get_or_gacc_( packed_layout_(), origin_addr );
    }

    // operator overloading for put/get
    // with user-defined (strided) origin buffers
    // NC stands for non-contiguous

    // TODO FIXME we rely on the user to *not* use
    // RMACXX_Subarray_t with elementwise operations.
    // At present, no compile time error is thrown,
    // is there a way to fix it?

    // overload istream op for bulk noncontiguous `put/acc
    // ---------------------------------------------------
    void operator <<( RMACXX_Subarray_t<T, GLOBAL_VIEW> const& origin )
    {
        // check to make sure the size of the subarray matches up with the window
        // store_lo and store_hi are inclusive coordinates on the Window
#ifdef RMACXX_SUBARRAY_USE_END_COORDINATES  // ---- USING END COORDINATES ----, we want to check limits differently when ends is used vs not
#ifdef DEBUG_CHECK_LIMITS
        // if we're given inclusive coordinates
        std::vector<int> nhi;
        nhi.insert(nhi.end(), store_hi.begin(), store_hi.end());
        if (origin.sizes_ != nhi) {
            //failed check
            std::cout << "Coordinates not equal" << std::endl;
            abort();
        }
#endif
#else                                       // ---- NOT USING END COORDINATES ----
#ifdef DEBUG_CHECK_LIMITS
        //if we're given size
        std::vector<int> nlo, nhi;
        nlo.insert(nlo.end(), store_lo.begin(), store_lo.end());
        nhi.insert(nhi.end(), store_hi.begin(), store_hi.end());

        //take all the hi and lo values and convert them into sizes
        std::vector<int> sizes(nhi.size());
        for (size_t i = 0; i < nlo.size(); i++) {
            sizes[i] = nhi[i] - nlo[i] + 1;
        }
        if (origin.sizes_ != sizes) {
            //failed check
            std::cout << "Coordinates not equal" << std::endl;
            abort();
        }
#endif
#endif
        put_or_acc_( subarray_layout_( origin ), origin.ptr_ );
    }

    // overloading ostream op for bulk noncontiguous `get
    // --------------------------------------------------
    void operator >>( RMACXX_Subarray_t<T, GLOBAL_VIEW> const& origin )
    {
        get_or_gacc_( subarray_layout_( origin ), origin.ptr_ );
    }

    /* Elementwise operations */
    /* ---------------------- */
    // overload istream op for element-wise `put/acc
    // ---------------------------------------------
    void operator <<( const T val )
    {
        put_or_acc_( detail::HostLayout::packed1( 1 ), &val );
    }

    // overloading ostream op for element-wise `get
    // OR fetch-and-op OR compare-and-swap
    // --------------------------------------------
    void operator >>( T& val )
    {
        if ( is_cas_ )
        {
            std::vector<detail::PatchDesc> pieces = pieces_();
            val = cas_val_; // compare value, replaced by the result
            eng_.issue( pieces, detail::HostLayout::packed1( 1 ), detail::Xfer::CAS, OpKind::NONE,
                        &val, &val, &fop_val_, true );
            is_cas_ = false;
            unlock();
        }
        else
            get_or_gacc_( detail::HostLayout::packed1( 1 ), &val );
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
            GWFLUSH();
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
            GWFLUSH();
        }

        eng_.complete_local();
    }

    // thwart attempts to copy objects
    // using standard assignment and parenthesis
    Window( const Window& ) = delete;
    Window& operator=( const Window& other ) = delete;

private:
    void init_( std::vector<int> const& lo, std::vector<int> const& hi, T* winbuf )
    {
        detail::ensure_init();

        if ( lo.size() != hi.size() || lo.empty() || lo.size() > RMACXX_MAX_DIMS )
            RMACXX_FATAL( "lo/hi must have the same size, in [1, RMACXX_MAX_DIMS]" );

        ndims_ = static_cast<int>( lo.size() );
        lo_ = lo;
        hi_ = hi;
        dims_.resize( ndims_ );
        nelems_ = 1;

        for ( int k = 0; k < ndims_; k++ )
        {
            dims_[k] = hi_[k] - lo_[k] + 1;

            if ( dims_[k] < 1 )
                RMACXX_FATAL( "window block must have hi >= lo in every dimension" );

            nelems_ *= dims_[k];
        }

        // exchange blocks of all PEs
        rlo_ = detail::Engine<T>::allgather( lo_.data(), ndims_ );
        rhi_ = detail::Engine<T>::allgather( hi_.data(), ndims_ );
        commRank_ = nvshmem_my_pe();
        commSize_ = nvshmem_n_pes();
        dims_per_pe_.resize( ndims_ * commSize_ );

        for ( int i = 0; i < ndims_ * commSize_; i++ )
            dims_per_pe_[i] = rhi_[i] - rlo_[i] + 1;

        is_same_wsize_ = true;

        for ( int i = 1; i < commSize_ && is_same_wsize_; i++ )
            for ( int k = 0; k < ndims_; k++ )
                if ( dims_per_pe_[k] != dims_per_pe_[i*ndims_+k] )
                {
                    is_same_wsize_ = false;
                    break;
                }

        // process grid: number of distinct block starts per dimension
        // (for 1D: 1 x npes, as before)
        if ( ndims_ == 1 )
            pgrid_ = { 1, commSize_ };
        else
        {
            pgrid_.assign( ndims_, 1 );

            for ( int k = 0; k < ndims_; k++ )
            {
                std::vector<int> starts;

                for ( int p = 0; p < commSize_; p++ )
                    starts.push_back( rlo_[p*ndims_+k] );

                std::sort( starts.begin(), starts.end() );
                pgrid_[k] = static_cast<int>( std::unique( starts.begin(), starts.end() ) - starts.begin() );
            }
        }

        pe_coords_.assign( ndims_ * commSize_, 0 );

        for ( int p = 0; p < commSize_; p++ )
            for ( int k = 0; k < ndims_; k++ )
            {
                int c = 0;

                for ( int q = 0; q < commSize_; q++ )
                    if ( rlo_[q*ndims_+k] < rlo_[p*ndims_+k] ) c++;

                pe_coords_[p*ndims_+k] = c;
            }

#ifdef DEBUG_CHECK_GAPS //checks gaps where we want to place, all spaces should be allocated to a process
        {
            std::vector<std::vector<int>> plos(commSize_), phis(commSize_);
            std::vector<int> dmin(ndims_), dmax(ndims_);
            std::vector<int> wsize(ndims_);

            for (int i = 0; i < ndims_; i++) {
                dmin[i] = INT32_MAX;
                dmax[i] = INT32_MIN;
            }

            for (int i = 0; i < ndims_ * commSize_; i++) {
                int process_index = i / ndims_;
                int dimension_index = i % ndims_;

                int h = rhi_[i];
                int l = rlo_[i];

                if (l < dmin[dimension_index]) dmin[dimension_index] = l;
                if (h > dmax[dimension_index]) dmax[dimension_index] = h;

                plos[process_index].push_back(l);
                phis[process_index].push_back(h);
            }

            for (int i = 0; i < ndims_; i++) {
                wsize[i] = dmax[i] - dmin[i];
            }

            bool no_gaps_exist = check_gaps(wsize, plos, phis);
            if (!no_gaps_exist) {
                throw -1;
            }
            std::cout<<(no_gaps_exist?"THERE ARE NO GAPS AT ALL":"There are gaps at all.")<<std::endl;
        }
#endif

        // device copies of the block table (for DeviceWindow)
        RMACXX_CUDA_CHECK( cudaMalloc( &d_rlo_, sizeof( int ) * ndims_ * commSize_ ) );
        RMACXX_CUDA_CHECK( cudaMalloc( &d_rhi_, sizeof( int ) * ndims_ * commSize_ ) );
        RMACXX_CUDA_CHECK( cudaMemcpy( d_rlo_, rlo_.data(), sizeof( int ) * ndims_ * commSize_, cudaMemcpyHostToDevice ) );
        RMACXX_CUDA_CHECK( cudaMemcpy( d_rhi_, rhi_.data(), sizeof( int ) * ndims_ * commSize_, cudaMemcpyHostToDevice ) );

        /* Allocate window */
        eng_.create( nelems_ );
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
            GW_RESERVE_EXPR_();
        }

        subsizes_.resize( ndims_ );
        starts_.resize( ndims_ );
        lsizes_.resize( ndims_ );
        lstarts_.assign( ndims_, 0 );
        iswinlocked_ = true;
        eng_.barrier();
    }

#ifdef RMACXX_USE_CLASSIC_HANDLES
    void GW_RESERVE_EXPR_() const {}
    void GW_CLEAR_EXPR_() const {}
#else
    void GW_RESERVE_EXPR_() const { expressions_.reserve( DEFAULT_EXPR_COUNT ); }
    void GW_CLEAR_EXPR_() const { expressions_.clear(); }
#endif

    // bulk access: global box [l, h]
    void set_box_( std::initializer_list<int> const& l, std::initializer_list<int> const& h )
    {
        if ( static_cast<int>( l.size() ) != ndims_ || static_cast<int>( h.size() ) != ndims_ )
            RMACXX_FATAL( "number of coordinates does not match window dimensionality" );

        elem_access_ = false;
        lo_.assign( l.begin(), l.end() );
        hi_.assign( h.begin(), h.end() );
        count_dimn_ = 1;

        for ( int k = 0; k < ndims_; k++ )
        {
            lsizes_[k] = hi_[k] - lo_[k] + 1;

            if ( lsizes_[k] < 1 )
                RMACXX_FATAL( "patch must have hi >= lo in every dimension" );

            count_dimn_ *= lsizes_[k];
        }
    }

    // descriptors for the current access (one per owning PE)
    std::vector<detail::PatchDesc> pieces_() const
    {
        std::vector<detail::PatchDesc> v;

        if ( elem_access_ )
        {
            v.push_back( detail::make_linear_desc( target_, disp_, 1,
                         static_cast<int64_t>( nelems_of_( target_ ) ) ) );
            return v;
        }

        int64_t covered = 0;

        for ( int p = 0; p < commSize_; p++ )
        {
            detail::PatchDesc d;

            if ( detail::make_global_piece( d, p, ndims_, rlo_.data(), rhi_.data(), lo_.data(), hi_.data() ) )
            {
                covered += d.count();
                v.push_back( d );
            }
        }

        // blocks may overlap (overlapping elements are replicated on every
        // owner, as before); fewer elements than requested means a gap
        if ( covered < count_dimn_ )
            RMACXX_FATAL( "patch is not fully covered by the window's blocks (gap)" );

        return v;
    }

    int64_t nelems_of_( int p ) const
    {
        int64_t n = 1;
        for ( int k = 0; k < ndims_; k++ ) n *= dims_per_pe_[p*ndims_+k];
        return n;
    }

    detail::HostLayout packed_layout_() const
    {
        if ( elem_access_ )
            return detail::HostLayout::packed1( 1 );

        return detail::HostLayout::packed( ndims_, lsizes_.data() );
    }

    detail::HostLayout subarray_layout_( RMACXX_Subarray_t<T, GLOBAL_VIEW> const& origin ) const
    {
        detail::HostLayout lay = packed_layout_();

        if ( ndims_ > 1 )
        {
            if ( origin.sizes_.size() != static_cast<size_t>( ndims_ ) || origin.starts_.size() != static_cast<size_t>( ndims_ ) )
                RMACXX_FATAL( "RMACXX_Subarray_t: starts/sizes must match window dimensionality" );

            for ( int k = 0; k < ndims_; k++ )
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
            // operand and result share the origin layout (as before)
            eng_.issue( pieces, lay, detail::Xfer::GACC, winop_.op, nullptr, origin, fop_inp_, false );
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

    bool iswinlocked_ = false, is_same_wsize_ = true;
    Op winop_;
    T* user_buf_ = nullptr;

    int ndims_ = 0;
    std::vector<int> dims_, dims_per_pe_; // dims per PE
    std::vector<int> pe_coords_, pgrid_;
    int64_t nelems_ = 0;

    // device copies of rlo_/rhi_
    int* d_rlo_ = nullptr;
    int* d_rhi_ = nullptr;

    // target and disp_ populated
    // in operator()() for elementwise
    int target_;
    int64_t disp_;
    bool elem_access_;

    // for bulk interface: current patch
    std::vector<int> lo_;
    std::vector<int> hi_;

    // stores data range per process
    std::vector<int> rlo_, rhi_;

#ifdef DEBUG_CHECK_LIMITS
    // stores the data range from the subarray constructor
    std::initializer_list<int> store_lo;
    std::initializer_list<int> store_hi;
#endif

    // values/pointers associated with CAS/FOP
    const T* fop_inp_;
    T fop_val_{};
    T cas_val_{};
    bool is_fop_, is_cas_;

#ifndef RMACXX_USE_CLASSIC_HANDLES
    mutable std::vector<exprid> expressions_;
#endif
    // parameters for subarray construction
    mutable std::vector<int> subsizes_, starts_;
    mutable std::vector<int> lsizes_, lstarts_;
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
}; // class Window<T,GLOBAL_VIEW,...>
