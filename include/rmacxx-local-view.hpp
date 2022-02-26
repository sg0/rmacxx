
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

    // TODO allow creation of empty windows, and define
    // methods to populate it later
public:
#define LWINIT_COMMON(dims) \
    do { \
        MPI_Comm_rank(comm_, &commRank_); \
        MPI_Comm_size(comm_, &commSize_); \
        ndims_ = dims.size(); \
        dims_.resize(ndims_); \
        memcpy(&(*(dims_.begin())), &(*(dims.begin())), ndims_ * sizeof(int)); \
        int nelems = 1; \
        for (int i = 0; i < ndims_; i++) nelems *= dims_[i]; \
        nelems_ = nelems; \
        /* populated by operator() */ \
        target_ = -1; \
        disp_ = 0; \
        fop_inp_ = nullptr; \
        cas_inp_ = nullptr; \
        is_fop_ = false; \
        is_cas_ = false; \
        winop_ = Op(); \
        winfo_ = MPI_INFO_NULL; \
        if (wuse_ == EXPR) \
        { \
            expr_past_info_1D_size_= 3*DEFAULT_EXPR_COUNT; \
            expr_issue_counter_    = 0; \
            expr_get_counter_      = 0; \
            expr_getND_counter_    = 0; \
            expr_size_counter_     = 0; \
            expr_xfer1D_counter_   = 0; \
            expr_xferND_counter_   = 0; \
            expr_buf_counter_      = 0; \
            expr_put_eval_counter_ = 0; \
            is_expr_elem_          = false; \
            is_expr_bulk_          = false; \
            expr_bptr_             = nullptr; \
            expr_info_1D_ = new int[expr_past_info_1D_size_]; \
            defer_put_xfer_1D_.reserve(DEFAULT_EXPR_COUNT); \
            defer_put_xfer_nD_.reserve(DEFAULT_EXPR_COUNT); \
        } \
        /* used for subarray construction for ndims > 1 */\
        subsizes_.resize(ndims_); \
        starts_.resize(ndims_); \
    } while(0)

    /* Win_allocate model */
    Window( std::vector<int> const& dims )
    {
        MPI_Comm_dup( MPI_COMM_WORLD, &comm_ );
        LWINIT_COMMON( dims );
        wkind_ = ALLOC;
#ifdef RMACXX_USE_SAME_SIZE_INFO
        MPI_Info_create( &winfo_ );
        MPI_Info_set( winfo_, "same_size", "true" );
#endif
        T* base = nullptr;
        MPI_Win_allocate( nelems_ * sizeof( T ),
                          sizeof( T ), winfo_, comm_, &base, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
    }

    Window( std::vector<int> const& dims, MPI_Comm comm )
    {
        MPI_Comm_dup( comm, &comm_ );
        LWINIT_COMMON( dims );
        wkind_ = ALLOC;
#ifdef RMACXX_USE_SAME_SIZE_INFO
        MPI_Info_create( &winfo_ );
        MPI_Info_set( winfo_, "same_size", "true" );
#endif
        T* base = nullptr;
        MPI_Win_allocate( nelems_ * sizeof( T ),
                          sizeof( T ), winfo_, comm_, &base, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
    }

    /* Win_create model, user passes pointer-to-buffer
     * and assumes ownership */
    Window( std::vector<int> const& dims, T* winbuf )
    {
        MPI_Comm_dup( MPI_COMM_WORLD, &comm_ );
        LWINIT_COMMON( dims );
        wkind_ = CREATE;
#ifdef RMACXX_USE_SAME_SIZE_INFO
        MPI_Info_create( &winfo_ );
        MPI_Info_set( winfo_, "same_size", "true" );
#endif
        MPI_Win_create( winbuf, nelems_ * sizeof( T ),
                        sizeof( T ), MPI_INFO_NULL, comm_, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
    }

    Window( std::vector<int> const& dims, MPI_Comm comm, T* winbuf )
    {
        MPI_Comm_dup( comm, &comm_ );
        LWINIT_COMMON( dims );
        wkind_ = CREATE;
#ifdef RMACXX_USE_SAME_SIZE_INFO
        MPI_Info_create( &winfo_ );
        MPI_Info_set( winfo_, "same_size", "true" );
#endif
        MPI_Win_create( winbuf, nelems_ * sizeof( T ),
                        sizeof( T ), MPI_INFO_NULL, comm_, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
    }

    // user has to call wfree to deallocate
    // resources
    ~Window() {}

    void wfree()
    {
        if ( iswinlocked_ )
        {
            MPI_Win_unlock_all( win_ );
            // if user calls wunlock, then it is user's onus
            // to call MPI_Win_free
            MPI_Win_free( &win_ );
            iswinlocked_ = false;
        }

#ifdef RMACXX_USE_SAME_SIZE_INFO
        MPI_Info_free( &winfo_ );
#endif
        subsizes_.clear();
        starts_.clear();
        dims_.clear();

        if ( wuse_ == EXPR )
        {
            delete []expr_info_1D_;
            defer_xfer_nD_.clear();
            defer_put_xfer_1D_.clear();
            defer_put_xfer_nD_.clear();
        }

        expr_bptr_ = nullptr;
        MPI_Comm_free( &comm_ );
    }

    // Window attributes
    inline MPI_Comm comm() { return comm_; }
    inline int ndims() const { return ndims_; }
    inline const std::vector<int>& dims() const { return dims_; }
    inline void barrier() { MPI_Barrier( comm_ ); }
    inline int size() const { return commSize_; }
    inline int rank() const { return commRank_; }
    inline WinCompletion completion() const { return wcmpl_; }
    // is my window locked?
    inline bool iswinlocked() const { return iswinlocked_; }

    // return a pointer to the window buffer
    inline T* wget() const
    {
        int flag = 0;
        T* wbaseptr = nullptr;
        MPI_Win_get_attr( win_, MPI_WIN_BASE, &wbaseptr, &flag );
        return wbaseptr;
    }

    // fill value in the buffer attached to the window
    inline void fill( T val ) const
    {
        T* localBuf = wget();

        for ( int i = 0; i < nelems_; i++ )
            localBuf[i] = val;

        MPI_Win_sync( win_ );
        MPI_Barrier( comm_ );
    }

    // returns 2d if ndims == 2,
    // otherwise returns data as per flat 1d layout
    void print( std::string const& s ) const
    {
        MPI_Barrier( comm_ );
        T* buf = wget();

        if ( commRank_ == 0 )
            std::cout << s << std::endl;

        if ( ndims_ == 2 ) // 2d layout
        {
            const int m = dims_[0];
            const int n = dims_[1];
            const int ld = n;

            for ( int p = 0; p < commSize_; p++ )
            {
                MPI_Barrier( comm_ );

                if ( p == commRank_ )
                {
                    std::cout << "[" << p << "]" << "dims: {" << m << "," << n << "}" << std::endl;

                    for ( int i = 0; i < m; i++ )
                    {
                        for ( int j = 0; j < n; j++ )
                            std::cout << " " << buf[i*ld + j] << " ";

                        std::cout << std::endl;
                    }

                    MPI_Barrier( comm_ );
                }
            }
        }
        else // 1d layout
        {
            for ( int p = 0; p < commSize_; p++ )
            {
                MPI_Barrier( comm_ );

                if ( p == commRank_ )
                {
                    std::cout << "[" << p << "]" << "dims: {" << nelems_ << "}" << std::endl;
                    for ( int i = 0; i < nelems_; i++ )
                        std::cout <<  buf[i] << " ";

                    std::cout << std::endl;
                }

                MPI_Barrier( comm_ );
            }
        }
    }

    // user unlocks window
    // takes control
    inline MPI_Win& wunlock()
    {
        if ( iswinlocked_ )
        {
            MPI_Win_unlock_all( win_ );
            iswinlocked_ = false;
        }

        return win_;
    }

    // user locks window
    // releases control
    // by default MPI_MODE_NOCHECK
    inline MPI_Win& wlock()
    {
        if ( !iswinlocked_ )
        {
            MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
            iswinlocked_ = true;
        }

        return win_;
    }

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
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, std::initializer_list<int> const&  hi, X )
    {
        lock();
        target_ = target;

        if ( ndims_ > 1 )
        {
            int i = 0;
            count_dimn_ = 1;
            auto ilo = lo.begin();
            std::for_each( hi.begin(), hi.end(), [this, &ilo, &i] ( int hi_val )
            {
                subsizes_[i] = hi_val - *ilo + 1;
                starts_[i] = *ilo;
                count_dimn_ *= subsizes_[i];
                i++;
                ilo++;
            } );
            disp_ = 0;
        }
        else
        {
            count_dimn_ = *( hi.begin() ) - *( lo.begin() ) + 1;
            disp_ = *( lo.begin() );
        }

        return *this;
    }

    inline WIN& operator()( int target, std::initializer_list<int> const& lo, X )
    {
        lock();
        target_ = target;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

        return *this;
    }
   
    // TODO FIXME this is only syntactic sugar, does this work
    // for all cases? if yes, also handle expression cases 
    inline WIN& operator()( int target, std::initializer_list<int64_t> const& lo, std::initializer_list<int64_t> const&  hi, X )
    {
        lock();
        target_ = target;

        if ( ndims_ > 1 )
        {
            int i = 0;
            count_dimn_ = 1;
            auto ilo = lo.begin();
            std::for_each( hi.begin(), hi.end(), [this, &ilo, &i] ( int64_t hi_val )
            {
                subsizes_[i] = (int)(hi_val - *ilo + 1);
                starts_[i] = (int)*ilo;
                count_dimn_ *= subsizes_[i];
                i++;
                ilo++;
            } );
            disp_ = 0;
        }
        else
        {
            count_dimn_ = (int)(*( hi.begin() ) - *( lo.begin() ) + 1);
            disp_ = *( lo.begin() );
        }

        return *this;
    }

    inline WIN& operator()( int target, std::initializer_list<int64_t> const& lo, X )
    {
        lock();
        target_ = target;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int64_t val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

        return *this;
    }
    
    // TODO FIXME handle large offsets
    // Window object wrapped with an
    // (element-wise/bulk) expression
    // object
    EExpr<T,RefEExpr<T,WIN>> operator()( int target, std::initializer_list<int> const& lo, Y )
    {
        lock();
        
        is_expr_elem_ = true;
        target_ = target;
        count_dimn_ = 1;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

        if ( expr_issue_counter_ >= expr_past_info_1D_size_ )
            resize_expr_info_1D_();

        expr_info_1D_[expr_issue_counter_] = target_;
        expr_info_1D_[expr_issue_counter_+1] = 1;
        expr_info_1D_[expr_issue_counter_+2] = disp_;
        expr_issue_counter_ += 3;
        
        unlock();
        
        return EExpr<T,RefEExpr<T,WIN>>( RefEExpr<T,WIN>( *this ) );
    }

    // post outstanding gets for prior element-wise
    // expression window access
    void eexpr_outstanding_gets() const
    {
        lock();

        if ( expr_buf_counter_ < PREALLOC_BUF_SZ )
        {
            if ( watmc_ == ATOMIC_PUT_GET )
                MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                    &pbuf_.buf_.buffer_[expr_buf_counter_],
                                    1, TypeMap<T>(),
                                    expr_info_1D_[expr_get_counter_],
                                    expr_info_1D_[expr_get_counter_+2],
                                    1, TypeMap<T>(), MPI_REPLACE, win_ );
            else
                MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                         1, TypeMap<T>(),
                         expr_info_1D_[expr_get_counter_],
                         expr_info_1D_[expr_get_counter_+2],
                         1, TypeMap<T>(), win_ );

            expr_get_counter_ += 3;
            expr_buf_counter_++;
        }

        unlock();
    }

    // recipe to resize expr_info_1D array which
    // in EXPR cases stores {target, count, disp}
    inline void resize_expr_info_1D_()
    {
        int new_sz = expr_issue_counter_ + ( 3*DEFAULT_EXPR_COUNT );
        int* new_expr_info_1D = new int[new_sz];
        memcpy( new_expr_info_1D, expr_info_1D_,
                expr_issue_counter_*sizeof( int ) );
        expr_past_info_1D_size_ = new_sz;
        delete [] expr_info_1D_;
        expr_info_1D_ = new_expr_info_1D;
    }

    BExpr<T,RefBExpr<T,WIN>> operator()( int target, std::initializer_list<int> const& lo, std::initializer_list<int> const& hi, Y )
    {
        lock();

        is_expr_bulk_ = true;
        target_ = target;

        if ( ndims_ > 1 )
        {
            count_dimn_ = 1;
            int i = 0;
            auto ilo = lo.begin();
            std::for_each( hi.begin(), hi.end(), [this, &ilo, &i] ( int hi_val )
            {
                subsizes_[i] = hi_val - *ilo + 1;
                starts_[i] = *ilo;
                count_dimn_ *= subsizes_[i];
                i++;
                ilo++;
            } );
            defer_xfer_nD_.emplace_back( target_, count_dimn_,
                                         subsizes_, starts_ );
        }
        else
        {
            count_dimn_ = *( hi.begin() ) - *( lo.begin() ) + 1;
            disp_ = *( lo.begin() );

            if ( expr_issue_counter_ >= expr_past_info_1D_size_ )
                resize_expr_info_1D_();

            expr_info_1D_[expr_issue_counter_] = target_;
            expr_info_1D_[expr_issue_counter_+1] = count_dimn_;
            expr_info_1D_[expr_issue_counter_+2] = disp_;
            expr_issue_counter_ += 3;
        }

        unlock();
        
        return BExpr<T,RefBExpr<T,WIN>>( RefBExpr<T,WIN>( *this ) );
    }

    // post outstanding gets for prior bulk
    // expression window access
    void bexpr_outstanding_gets() const
    {
        lock();

        // post gets if space available on buffer
        if ( expr_buf_counter_ < PREALLOC_BUF_SZ )
        {
            if ( watmc_ == ATOMIC_PUT_GET )
            {
                if ( ndims_ == 1 )
                {
                    int const& count = expr_info_1D_[expr_get_counter_+1];

                    if ( expr_buf_counter_ == 0 )
                    {
                        // for the very first time, requested data may be
                        // greater than available preallocated buffer space
                        if ( ( expr_buf_counter_ + count ) < PREALLOC_BUF_SZ )
                        {
                            MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                                &pbuf_.buf_.buffer_[expr_buf_counter_],
                                                count, TypeMap<T>(),
                                                expr_info_1D_[expr_get_counter_],
                                                expr_info_1D_[expr_get_counter_+2],
                                                count, TypeMap<T>(), MPI_REPLACE, win_ );
                            expr_get_counter_ += 3;
                            expr_buf_counter_ += count;
                        }
                    }
                    else
                    {
                        MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                            &pbuf_.buf_.buffer_[expr_buf_counter_],
                                            count, TypeMap<T>(),
                                            expr_info_1D_[expr_get_counter_],
                                            expr_info_1D_[expr_get_counter_+2],
                                            count, TypeMap<T>(), MPI_REPLACE, win_ );
                        expr_get_counter_ += 3;
                        expr_buf_counter_ += count;
                    }
                }
                else
                {
                    int const& count = defer_xfer_nD_[expr_getND_counter_].count_;

                    if ( expr_buf_counter_ == 0 )
                    {
                        if ( ( expr_buf_counter_ + count ) < PREALLOC_BUF_SZ )
                        {
                            MPI_Datatype sarr_type;
                            MPI_Type_create_subarray( ndims_, dims_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                      MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                            MPI_Type_commit( &sarr_type );
                            MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                                &pbuf_.buf_.buffer_[expr_buf_counter_],
                                                count, TypeMap<T>(), defer_xfer_nD_[expr_getND_counter_].target_,
                                                /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_ );
                            MPI_Type_free( &sarr_type );
                            expr_buf_counter_ += count;
                            expr_getND_counter_++;
                        }
                    }
                    else
                    {
                        MPI_Datatype sarr_type;
                        MPI_Type_create_subarray( ndims_, dims_.data(),
                                                  defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                  defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                  MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                        MPI_Type_commit( &sarr_type );
                        MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                            &pbuf_.buf_.buffer_[expr_buf_counter_],
                                            count, TypeMap<T>(), defer_xfer_nD_[expr_getND_counter_].target_,
                                            /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_ );
                        MPI_Type_free( &sarr_type );
                        expr_buf_counter_ += count;
                        expr_getND_counter_++;
                    }
                }
            }
            else
            {
                if ( ndims_ == 1 )
                {
                    int const& count = expr_info_1D_[expr_get_counter_+1];

                    if ( expr_buf_counter_ == 0 )
                    {
                        if ( ( expr_buf_counter_ + count ) < PREALLOC_BUF_SZ )
                        {
                            MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                                     count, TypeMap<T>(),
                                     expr_info_1D_[expr_get_counter_],
                                     expr_info_1D_[expr_get_counter_+2],
                                     count, TypeMap<T>(), win_ );
                            expr_buf_counter_ += count;
                            expr_get_counter_ += 3;
                        }
                    }
                    else
                    {
                        MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                                 count, TypeMap<T>(),
                                 expr_info_1D_[expr_get_counter_],
                                 expr_info_1D_[expr_get_counter_+2],
                                 count, TypeMap<T>(), win_ );
                        expr_buf_counter_ += count;
                        expr_get_counter_ += 3;
                    }
                }
                else
                {
                    int const& count = defer_xfer_nD_[expr_getND_counter_].count_;

                    if ( expr_buf_counter_ == 0 )
                    {
                        if ( ( expr_buf_counter_ + count ) < PREALLOC_BUF_SZ )
                        {
                            MPI_Datatype sarr_type;
                            MPI_Type_create_subarray( ndims_, dims_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                      MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                            MPI_Type_commit( &sarr_type );
                            MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                                     count, TypeMap<T>(),
                                     defer_xfer_nD_[expr_getND_counter_].target_,/*disp*/ 0, 1,
                                     sarr_type, win_ );
                            MPI_Type_free( &sarr_type );
                            expr_buf_counter_ += count;
                            expr_getND_counter_++;
                        }
                    }
                    else
                    {
                        MPI_Datatype sarr_type;
                        MPI_Type_create_subarray( ndims_, dims_.data(),
                                                  defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                  defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                  MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                        MPI_Type_commit( &sarr_type );
                        MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                                 count, TypeMap<T>(),
                                 defer_xfer_nD_[expr_getND_counter_].target_,/*disp*/ 0, 1,
                                 sarr_type, win_ );
                        MPI_Type_free( &sarr_type );
                        expr_buf_counter_ += count;
                        expr_getND_counter_++;
                    }
                }
            }
        }

        unlock();
    }

    // ignore get, since the communication
    // params are going to be used for a put
    inline void expr_ignore_last_get() const
    {
        lock();

        if ( ndims_ == 1 )
        {
            // counter adjustment
            expr_issue_counter_ -= 3;
            defer_put_xfer_1D_.emplace_back(
                expr_info_1D_[expr_issue_counter_],
                expr_info_1D_[expr_issue_counter_+1],
                expr_info_1D_[expr_issue_counter_+2] );
        }
        else
        {
            const int idx = defer_xfer_nD_.size()-1;
            defer_put_xfer_nD_.emplace_back( defer_xfer_nD_[idx].target_,
                                             defer_xfer_nD_[idx].count_,
                                             defer_xfer_nD_[idx].subsizes_,
                                             defer_xfer_nD_[idx].starts_ );
            defer_xfer_nD_.pop_back();
        }

        unlock();
    }

    // puts

    // this is required for EXPR cases when there is
    // a window on the RHS to >>
    // we only need to access the window parameters
    // of the last window object for puts

    // by default, this is locally blocking, which
    // does not matter for NO_FLUSH situations, as
    // the entire function will be deferred until
    // user calls flush
    void eexpr_outstanding_put( const T val ) const
    {
        lock();

        if ( watmc_ == ATOMIC_PUT_GET )
            MPI_Accumulate( &val, 1, TypeMap<T>(),
                            defer_put_xfer_1D_[expr_put_eval_counter_].target_,
                            defer_put_xfer_1D_[expr_put_eval_counter_].disp_,
                            1, TypeMap<T>(), MPI_REPLACE, win_ );
        else
            MPI_Put( &val, 1, TypeMap<T>(),
                     defer_put_xfer_1D_[expr_put_eval_counter_].target_,
                     defer_put_xfer_1D_[expr_put_eval_counter_].disp_,
                     1, TypeMap<T>(), win_ );

        MPI_Win_flush_local
        ( defer_put_xfer_1D_[expr_put_eval_counter_].target_, win_ );
        expr_put_eval_counter_++;
        
        unlock();
    }

    void bexpr_outstanding_put( const T* origin_addr ) const
    {
        lock();

        if ( watmc_ == ATOMIC_PUT_GET )
        {
            if ( ndims_ == 1 )
                MPI_Accumulate( origin_addr, defer_put_xfer_1D_[expr_put_eval_counter_].count_,
                                TypeMap<T>(), defer_put_xfer_1D_[expr_put_eval_counter_].target_,
                                disp_, defer_put_xfer_1D_[expr_put_eval_counter_].count_,
                                TypeMap<T>(), MPI_REPLACE, win_ );
            else
            {
                MPI_Datatype sarr_type;
                MPI_Type_create_subarray( ndims_, dims_.data(),
                                          defer_put_xfer_nD_[expr_put_eval_counter_].subsizes_.data(),
                                          defer_put_xfer_nD_[expr_put_eval_counter_].starts_.data(),
                                          MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                MPI_Type_commit( &sarr_type );
                MPI_Accumulate( origin_addr, defer_put_xfer_nD_[expr_put_eval_counter_].count_,
                                TypeMap<T>(), defer_put_xfer_nD_[expr_put_eval_counter_].target_,
                                /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_ );
                MPI_Type_free( &sarr_type );
            }

            MPI_Win_flush_local
            ( defer_put_xfer_1D_[expr_put_eval_counter_].target_, win_ );
        }
        else
        {
            if ( ndims_ == 1 )
            {
                MPI_Put( origin_addr, defer_put_xfer_1D_[expr_put_eval_counter_].count_,
                         TypeMap<T>(), defer_put_xfer_1D_[expr_put_eval_counter_].target_,
                         defer_put_xfer_1D_[expr_put_eval_counter_].disp_,
                         defer_put_xfer_1D_[expr_put_eval_counter_].count_,
                         TypeMap<T>(), win_ );
                MPI_Win_flush_local
                ( defer_put_xfer_1D_[expr_put_eval_counter_].target_, win_ );
            }
            else
            {
                MPI_Datatype sarr_type;
                MPI_Type_create_subarray( ndims_, dims_.data(),
                                          defer_put_xfer_nD_[expr_put_eval_counter_].subsizes_.data(),
                                          defer_put_xfer_nD_[expr_put_eval_counter_].starts_.data(),
                                          MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                MPI_Type_commit( &sarr_type );
                MPI_Put( origin_addr, defer_put_xfer_nD_[expr_put_eval_counter_].count_,
                         TypeMap<T>(), defer_put_xfer_nD_[expr_put_eval_counter_].target_,
                         /*disp*/ 0, 1, sarr_type, win_ );
                MPI_Type_free( &sarr_type );
                MPI_Win_flush_local
                ( defer_put_xfer_nD_[expr_put_eval_counter_].target_, win_ );
            }
        }

        expr_put_eval_counter_++;
        
        unlock();
    }

    // for acc
    // -------
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, std::initializer_list<int> const& hi, Op op )
    {
        lock();
        
        target_ = target;
        winop_ = op;

        if ( ndims_ > 1 )
        {
            count_dimn_ = 1;
            int i = 0;
            auto ilo = lo.begin();
            std::for_each( hi.begin(), hi.end(), [this, &ilo, &i] ( int hi_val )
            {
                subsizes_[i] = hi_val - *ilo + 1;
                starts_[i] = *ilo;
                count_dimn_ *= subsizes_[i];
                i++;
                ilo++;
            } );
            disp_ = 0;
        }
        else
        {
            count_dimn_ = *( hi.begin() ) - *( lo.begin() ) + 1;
            disp_ = *( lo.begin() );
        }

        return *this;
    }

    // TODO FIXME handle large offsets for all operations
    // wrap the input std::init_list into Lo/Hi classes like before
    // that can accept either int or int64_t in compile time, to
    // avoid such duplication of code
    
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, Op op )
    {
        lock();
        
        target_ = target;
        winop_ = op;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

        return *this;
    }
 
    inline WIN& operator()( int target, std::initializer_list<int64_t> const& lo, Op op )
    {
        lock();
        
        target_ = target;
        winop_ = op;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int64_t val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

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
        fop_inp_ = const_cast<T*>( inpval );

        if ( ndims_ > 1 )
        {
            count_dimn_ = 1;
            int i = 0;
            auto ilo = lo.begin();
            std::for_each( hi.begin(), hi.end(), [this, &ilo, &i] ( int hi_val )
            {
                subsizes_[i] = hi_val - *ilo + 1;
                starts_[i] = *ilo;
                count_dimn_ *= subsizes_[i];
                i++;
                ilo++;
            } );
            disp_ = 0;
        }
        else
        {
            count_dimn_ = *( hi.begin() ) - *( lo.begin() ) + 1;
            disp_ = *( lo.begin() );
        }

        return *this;
    }

    // for fetch-and-op
    // ----------------
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, Op op, T inpval )
    {
        lock();
        
        target_ = target;
        winop_ = op;
        fop_inp_ = &inpval;
        is_fop_ = true;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

        return *this;
    }

    // for compare-and-swap
    // --------------------
    inline WIN& operator()( int target, std::initializer_list<int> const& lo, const T inpval, const T cmpval )
    {
        lock();
        
        target_ = target;
        fop_inp_ = const_cast<T*>(&inpval);
        cas_inp_ = const_cast<T*>(&cmpval);
        is_cas_ = true;

        if ( ndims_ > 1 )
        {
            disp_ = 0;
            int idx = 0;
            std::for_each( lo.begin(), lo.end(), [this, &idx] ( int val )
            {
                for ( int i = ndims_-1; i >= idx+1; i-- )  val *= dims_[i];

                disp_ += val;
                idx++;
            } );
        }
        else
            disp_ = *( lo.begin() );

        return *this;
    }

    // pushes+completes
    // any pending deferred gets
    T eval() const
    {
        T val = T( 0 );
        
        lock();
        
        int& target = expr_info_1D_[expr_xfer1D_counter_];

        if ( expr_size_counter_ < PREALLOC_BUF_SZ )
        {
            flush_local( target );
            // this position is flushed, and could be reused
            // encoding the index to preallocated buffer such
            // that it is easier to index later
            // this will not work for the very first position (0)
            target = ( -1 )*expr_size_counter_;
            val = pbuf_.buf_.buffer_[expr_size_counter_];
        }
        else // issue+complete pending gets
        {
            if ( watmc_ == ATOMIC_PUT_GET )
                MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL, &val,
                                    1, TypeMap<T>(), target,
                                    expr_info_1D_[expr_xfer1D_counter_+2],
                                    1, TypeMap<T>(), MPI_REPLACE, win_ );
            else
                MPI_Get( &val, 1, TypeMap<T>(), target,
                         expr_info_1D_[expr_xfer1D_counter_+2],
                         1, TypeMap<T>(), win_ );

            flush_local( target );
        }

        expr_size_counter_++;
        expr_xfer1D_counter_ += 3;
        
        unlock();
        
        return val;
    }

    // return the last count
    inline int get_count() const { return count_dimn_; }

    // bulk expression evaluation
    // returns the count of elements processed in get
    int fillInto( T* buf ) const
    {
        int count = 0;
        
        lock();

        if ( ndims_ > 1 )
        {
            // check if preallocated buffer was used in a previously
            // issued get
            count = defer_xfer_nD_[expr_xferND_counter_].count_;

            if ( expr_size_counter_ == 0 && ( ( expr_size_counter_ + count ) < PREALLOC_BUF_SZ ) )
            {
                // flush on MPI_PROC_NULL throws an error, so trying to prevent it
                flush_local( defer_xfer_nD_[expr_xferND_counter_].target_ );
                memcpy( buf, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                expr_size_counter_ += count;
            }
            else if ( expr_size_counter_ < PREALLOC_BUF_SZ )
            {
                flush_local( defer_xfer_nD_[expr_xferND_counter_].target_ );
                defer_xfer_nD_[expr_xferND_counter_].target_ = ( -1 )*expr_size_counter_;
                memcpy( buf, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                expr_size_counter_ += count;
            }
            else // issue+complete gets
            {
                int const& target = defer_xfer_nD_[expr_xferND_counter_].target_;
                MPI_Datatype sarr_type;
                MPI_Type_create_subarray( ndims_, dims_.data(),
                                          defer_xfer_nD_[expr_xferND_counter_].subsizes_.data(),
                                          defer_xfer_nD_[expr_xferND_counter_].starts_.data(),
                                          MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                MPI_Type_commit( &sarr_type );

                if ( watmc_ == ATOMIC_PUT_GET )
                {
                    MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL, buf,
                                        count, TypeMap<T>(), target,/*disp*/ 0, 1,
                                        sarr_type, MPI_REPLACE, win_ );
                }
                else
                {
                    MPI_Get( buf, count, TypeMap<T>(),
                             target, /*disp*/ 0, 1, sarr_type, win_ );
                }

                MPI_Type_free( &sarr_type );
                flush_local( target );
            }

            expr_xferND_counter_++;
        } // end of ndims > 1
        else // ndims == 1
        {
            // check if preallocated buffer was used in a previously
            // issued get
            count = expr_info_1D_[expr_xfer1D_counter_+1];

            if ( expr_size_counter_ == 0 && ( ( expr_size_counter_ + count ) < PREALLOC_BUF_SZ ) )
            {
                flush_local( expr_info_1D_[expr_xfer1D_counter_] );
                memcpy( buf, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                expr_size_counter_ += count;
            }
            else if ( expr_size_counter_ < PREALLOC_BUF_SZ )
            {
                flush_local( expr_info_1D_[expr_xfer1D_counter_] );
                expr_info_1D_[expr_xfer1D_counter_] = ( -1 )*expr_size_counter_;
                memcpy( buf, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                expr_size_counter_ += count;
            }
            else // issue+complete gets
            {
                int const& target = expr_info_1D_[expr_xfer1D_counter_];

                if ( watmc_ == ATOMIC_PUT_GET )
                {
                    MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL, buf,
                                        count, TypeMap<T>(), target,
                                        expr_info_1D_[expr_xfer1D_counter_+2],
                                        count, TypeMap<T>(), MPI_REPLACE, win_ );
                }
                else
                {
                    MPI_Get( buf, count, TypeMap<T>(), target,
                             expr_info_1D_[expr_xfer1D_counter_+2],
                             count, TypeMap<T>(), win_ );
                }

                flush_local( target );
            }

            expr_xfer1D_counter_ += 3;
        } // end of ndims == 1

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
        defer_xfer_nD_.clear();
        defer_put_xfer_nD_.clear();
        defer_put_xfer_1D_.clear();
        expr_get_counter_       = 0;
        expr_getND_counter_     = 0;
        expr_xfer1D_counter_    = 0;
        expr_xferND_counter_    = 0;
        expr_size_counter_      = 0;
        expr_buf_counter_       = 0;
        expr_put_eval_counter_  = 0;
        expr_issue_counter_     = 0;
        expr_past_info_1D_size_ = ( 3*DEFAULT_EXPR_COUNT );
        expr_bptr_              = nullptr;
        is_expr_elem_           = false;
        is_expr_bulk_           = false;
    }

    // operators overloading for standard
    // put/get
    // overload istream op for bulk `put/acc
    // -------------------------------------
    void operator <<( const T* origin_addr )
    {
        if ( winop_.op == MPI_OP_NULL )
            RMACXX_BULK_PUT( origin_addr );
        else
            RMACXX_BULK_ACC( origin_addr );
    }

    // overloading ostream op for bulk `get
    // ------------------------------------
    void operator >>( T* origin_addr )
    {
        if ( is_fop_ )
            RMACXX_BULK_GACC( origin_addr );
        else
            RMACXX_BULK_GET( origin_addr );
    }

    // operator overloading for put/get
    // with user-defined types
    // NC stands for non-contiguous
    // TODO FIXME although Subarray_t only makes sense 
    // for bulk cases, if someone uses elementwise
    // interface on the LHS, the compiler will not flag
    // it as an error currently, fix that

    // overload istream op for bulk noncontiguous `put/acc
    // ---------------------------------------------------
    void operator <<( RMACXX_Subarray_t<T, LOCAL_VIEW>& origin )
    {
        if ( winop_.op == MPI_OP_NULL )
            RMACXX_BULK_PUT_NC( origin );
        else
            RMACXX_BULK_ACC_NC( origin );
    }

    // TODO FIXME we rely on the user to *not* use
    // RMACXX_Subarray_t with elementwise operations.
    // At present, no compile time error is thrown,
    // is there a way to fix it?

    // overloading ostream op for bulk noncontiguous `get
    // --------------------------------------------------
    void operator >>( RMACXX_Subarray_t<T, LOCAL_VIEW>& origin )
    {
        if ( is_fop_ )
            RMACXX_BULK_GACC_NC( origin );
        else
            RMACXX_BULK_GET_NC( origin );
    }

    // overload istream op for element-wise `put/acc
    // ---------------------------------------------
    void operator <<( const T val )
    {
        if ( winop_.op == MPI_OP_NULL )
            RMACXX_ELEM_PUT( val );
        else
            RMACXX_ELEM_ACC( val );
    }

    // overloading ostream op for element-wise `get
    // OR fetch-and-op
    // --------------------------------------------
    void operator >>( T& val )
    {
        if ( is_fop_ )
#ifdef RMACXX_USE_MPI_FOP_TO_IMPL_FOP
            RMACXX_FOP_ALT( val );
#else
            RMACXX_FOP( val );
#endif
        else if ( is_cas_ )
            RMACXX_CAS( val );
        else
            RMACXX_ELEM_GET( val );
    }

    // synchronization
    inline void flush_local( int const& target ) const
    {
#ifdef TEST_OVERHEAD
#else
        MPI_Win_flush_local( target, win_ );
#endif
    }
    inline void flush( int const& target ) const
    {
#ifdef TEST_OVERHEAD
#else
        MPI_Win_flush( target, win_ );
#endif
    }
    // for flushing outstanding put for EXPR cases when window
    // is in RHS to >>
    inline void sync() const { MPI_Win_sync( win_ ); }

    inline void flush() const { flush( std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }
    inline void flush_local() const { flush_local( std::conditional_t<( wuse_==NO_EXPR ), X, Y> {} ); }

    inline void flush( X ) const
    {
#ifdef TEST_OVERHEAD
#else
        MPI_Win_flush_all( win_ );
#endif
    }
    inline void flush_local( X ) const
    {
#ifdef TEST_OVERHEAD
#else
        MPI_Win_flush_local_all( win_ );
#endif
    }

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
            // flush is a static member in (B|E)Expr
            if ( is_expr_elem_ ) EExpr<T, WIN>::flush();

            if ( is_expr_bulk_ ) BExpr<T, WIN>::flush();

            flush_expr();
        }
    }

    // this is to make it easy to just call flush_all
    // from the expression classes
    inline void flush_win() const
    {
#ifdef TEST_OVERHEAD
#else
        MPI_Win_flush_all( win_ );
#endif
    }
    inline void flush_win_local() const
    {
#ifdef TEST_OVERHEAD
#else
        MPI_Win_flush_local_all( win_ );
#endif
    }

    inline void flush_local( Y ) const
    {
        if ( wcmpl_ == NO_FLUSH )
        {
            if ( is_expr_elem_ ) EExpr<T, WIN>::flush();

            if ( is_expr_bulk_ ) BExpr<T, WIN>::flush();

            flush_expr();
        }
    }

    // thwart attempts to copy objects
    // using standard assignment and parenthesis
    Window( const Window& ) = delete;
    Window& operator=( const Window& other ) = delete;

private:
    // buffer for temporary storage,
    // only used for EXPR cases
    struct Buffer_t { std::array<T, PREALLOC_BUF_SZ> buffer_; };
    struct Buffer
    { typename std::conditional<wuse_==EXPR, Buffer_t, X>::type buf_; };

    struct ExprDefer_nD
    {
        int target_, count_;
        std::vector<int> subsizes_;
        std::vector<int> starts_;

        ExprDefer_nD( int target, int count,
                      std::vector<int> subsizes,
                      std::vector<int> starts
                    ):
            target_( target ), count_( count ),
            subsizes_( subsizes ), starts_( starts )
        {}
    };

    // for issuing puts, case when window
    // is at the RHS of an expression
    struct ExprDeferPut_1D
    {
        int target_, count_;
        MPI_Aint disp_;

        ExprDeferPut_1D( int target, int count, MPI_Aint disp ):
            target_( target ), count_( count ), disp_( disp )
        {}

        ExprDeferPut_1D( int target, MPI_Aint disp ):
            target_( target ), count_( 1 ), disp_( disp )
        {}
    };

    struct ExprDeferPut_nD
    {
        int target_, count_;
        std::vector<int> subsizes_;
        std::vector<int> starts_;

        ExprDeferPut_nD( int target, int count,
                         std::vector<int> subsizes,
                         std::vector<int> starts
                       ):
            target_( target ), count_( count ),
            subsizes_( subsizes ), starts_( starts )
        {}
    };

    // ------------------- //

    MPI_Comm comm_;
    int commRank_;
    int commSize_;

    mutable MPI_Win win_;
    bool iswinlocked_;
    // Win kind, win_create or win_alloc'd
    WinKind wkind_;
    Op winop_;
    MPI_Info winfo_;

    int ndims_;
    std::vector<int> dims_;
    int nelems_;

    mutable int target_;
    // target displacement for element-wise
    // operations, calculated directly
    // from operator()
    mutable MPI_Aint disp_;

    // stores various pointers
    // associated with CAS/FOP
    T* fop_inp_;
    T* cas_inp_;
    bool is_fop_, is_cas_;

    mutable std::vector<int> subsizes_;
    mutable std::vector<int> starts_;
    mutable int count_dimn_;

    // counters to track issue/evaluation
    mutable int expr_size_counter_,
            expr_past_info_1D_size_,
            expr_issue_counter_,
            expr_get_counter_,
            expr_getND_counter_,
            expr_xfer1D_counter_,
            expr_xferND_counter_,
            expr_buf_counter_,
            expr_put_eval_counter_;

    // store issued counts for window objects
    // used in expressions

    // for storing {target,count,disp} for 1D
    // EXPR cases
    // allocated to (3*DEFAULT_EXPR_COUNT), and
    // resized as needed
    mutable int* expr_info_1D_;
    mutable std::vector<ExprDefer_nD> defer_xfer_nD_;
    mutable std::vector<ExprDeferPut_1D> defer_put_xfer_1D_;
    mutable std::vector<ExprDeferPut_nD> defer_put_xfer_nD_;

    mutable bool is_expr_elem_, is_expr_bulk_;

    // statically allocated buffer of default
    // size per window
    mutable Buffer pbuf_;
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
    mutable std::mutex win_mtx_;
    void lock() const { if ( wtsft_ == CONCURRENT ) win_mtx_.lock(); }
    void unlock() const { if ( wtsft_ == CONCURRENT ) win_mtx_.unlock(); }
#endif
}; // class Window<T,LOCAL_VIEW,...>

