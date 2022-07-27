// TODO FIXME finish coding RMACXX_USE_MPI_AINT_FOR_COORD.
// This is for cases when input coordinates are greater
// than INT range (count is still INT).

// TODO allow creation of empty windows, and define
// methods to populate it later

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
    #define GW_RESERVE_EXPR() {}
    #define GW_CLEAR_EXPR() {}
#else
    //TODO: FIX AND REIMPLEMENT FLUSH
    #define GWFLUSH() do{ \
        for(auto expr:expressions_){ \
            FuturesManager<T>::instance().unblock_expr(expr);\
        }                 \
        flush_expr();\
    } while(0)
    #define GW_RESERVE_EXPR() expressions_.reserve(DEFAULT_EXPR_COUNT)
    #define GW_CLEAR_EXPR() expressions_.clear()
#endif

public:
    // TODO FIXME removed cart_comm ctor for now due to some bugs
    // shall bring it back soon after clean up

    // stores all the lo's and hi's, packaged together for each process
    std::vector<std::vector<int>> los;
    std::vector<std::vector<int>> his;

#define GWINIT_COMMON_RANGE(lo, hi) \
    do { \
        /* calculate dims */ \
        ndims_ = 0; \
        nelems_ = 1; \
        auto ilo = lo.begin(); \
        std::for_each( hi.begin(), hi.end(), [this, &ilo] ( int hi_val ) \
                { \
                lo_.push_back( *ilo ); \
                hi_.push_back( hi_val ); \
                dims_.emplace_back( hi_[ndims_] - lo_[ndims_] + 1 ); \
                nelems_ *= dims_[ndims_]; \
                ilo++; \
                ndims_++; \
                } ); \
        MPI_Comm_rank(comm_, &commRank_); \
        MPI_Comm_size(comm_, &commSize_); \
        rlo_.reserve(ndims_*commSize_); \
        rhi_.reserve(ndims_*commSize_); \
        dims_per_pe_.reserve(ndims_*commSize_); \
        MPI_Allgather(dims_.data(), ndims_, MPI_INT, \
                dims_per_pe_.data(), ndims_, MPI_INT, comm_); \
        MPI_Allgather(lo_.data(), ndims_, MPI_INT, rlo_.data(), \
                ndims_, MPI_INT, comm_); \
        MPI_Allgather(hi_.data(), ndims_, MPI_INT, rhi_.data(), \
                ndims_, MPI_INT, comm_); \
        if (ndims_ == 1) \
        { \
            /* No need to create cart_comm for 1D */\
            pe_coords_.reserve(2*commSize_); \
            std::vector<int> pe_coord(2); \
            pgrid_.reserve(2); \
            pgrid_[0] = 1; \
            pgrid_[1] = commSize_; \
            pe_coord[0] = 0; \
            pe_coord[1] = commRank_; \
            MPI_Allgather(pe_coord.data(), 2, MPI_INT, \
                    pe_coords_.data(), 2, MPI_INT, comm_); \
            pe_coord.clear(); \
            cart_comm_ = MPI_COMM_NULL; \
        } \
        else \
        { \
            pe_coords_.reserve(ndims_*commSize_); \
            /* determine PE grid dimensionality */ \
            is_pgrid_unidir_ = true; \
            bool pgrid_lastdim = true; \
            /* TODO FIXME high priority: get rid of this 
             * we should be able to have a single unit
             * to compute grid dims
             */\
            for (int i = 1; ( ( i < commSize_ ) && ( is_pgrid_unidir_ ) ) ; i++) \
            { \
                for (int k = 0; k < ( ndims_ - 1 ); k++) \
                { \
                    if ( rlo_[(i-1)*ndims_ + k] != rlo_[i*ndims_ + k] ) \
                    { \
                        is_pgrid_unidir_ = false; \
                            pgrid_lastdim = false; \
                            break; \
                    } \
                } \
            } \
            if ( pgrid_lastdim ) \
                pgrid_unidx_ = ndims_ - 1; \
            else \
            { \
                is_pgrid_unidir_ = true; \
                    bool pgrid_firstdim = true; \
                    for (int i = 1; ( ( i < commSize_ ) && ( is_pgrid_unidir_ ) ) ; i++) \
                    { \
                        for (int k = 1; k < ndims_; k++) \
                        { \
                            if ( rlo_[(i-1)*ndims_ + k] != rlo_[i*ndims_ + k] ) \
                            { \
                                is_pgrid_unidir_ = false; \
                                    pgrid_firstdim = false; \
                                    break; \
                            } \
                        } \
                    } \
                if ( pgrid_firstdim ) \
                    pgrid_unidx_ = 0; \
            } \
            /* construct PE grid and cartesian comm */ \
            /* TODO FIXME remove hardcoding */\
            if ( is_pgrid_unidir_ ) \
            { \
                std::vector<int> pe_coord, periods; \
                pe_coord.resize(ndims_); \
                pgrid_.resize(ndims_); \
                periods.resize(ndims_); \
                for (int j = 0; j < ndims_-1; j++) \
                    periods[j] = 1; \
                /* construct grid dimensions */\
                for (int k = 0; k < ndims_; k++) \
                { \
                    for (int i = 1; i < commSize_; i++) \
                    { \
                        if (rlo_[i*ndims_+k] > rlo_[(i-1)*ndims_+k]) \
                            pgrid_[k]++; \
                        if (rlo_[i*ndims_+k] < rlo_[(i-1)*ndims_+k]) \
                            pgrid_[k]--; \
                    } \
                    pgrid_[k]++; \
                } \
                MPI_Cart_create(comm_, ndims_, pgrid_.data(), \
                        periods.data(), 0 /* reorder ranks */, &cart_comm_); \
                MPI_Cart_coords(cart_comm_, commRank_, ndims_, pe_coord.data()); \
                MPI_Allgather(pe_coord.data(), ndims_, MPI_INT, \
                        pe_coords_.data(), ndims_, MPI_INT, comm_); \
                periods.clear(); \
                pe_coord.clear(); \
            } \
            else \
            { \
                std::vector<int> pe_coord, periods; \
                pgrid_.resize(ndims_, 0); \
                periods.resize(ndims_); \
                for (int j = 0; j < ndims_-1; j++) \
                    periods[j] = 1; \
                MPI_Dims_create(commSize_, ndims_, pgrid_.data()); \
                MPI_Cart_create(comm_, ndims_, pgrid_.data(), \
                        periods.data(), 0 /* reorder ranks */, &cart_comm_); \
                pe_coord.resize(ndims_); \
                MPI_Cart_coords(cart_comm_, commRank_, ndims_, pe_coord.data()); \
                MPI_Allgather(pe_coord.data(), ndims_, MPI_INT, \
                        pe_coords_.data(), ndims_, MPI_INT, comm_); \
                periods.clear(); \
                pe_coord.clear(); \
            } \
        } \
        is_same_wsize_ = true; \
        for (int i = 1; i < commSize_ && is_same_wsize_; i++) \
        { \
            for (int k = 0; k < ndims_; k++) \
            { \
                if (dims_per_pe_[k] != dims_per_pe_[i*ndims_+k]) \
                { \
                    is_same_wsize_ = false; \
                    break; \
                } \
            } \
        } \
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
            expr_past_info_1D_size_         = 3*DEFAULT_EXPR_COUNT; \
            expr_issue_counter_             = 0; \
            expr_get_counter_               = 0; \
            expr_getND_counter_             = 0; \
            expr_size_counter_              = 0; \
            expr_xfer1D_counter_            = 0; \
            expr_xferND_counter_            = 0; \
            expr_buf_counter_               = 0; \
            expr_put_eval_counter_          = 0; \
            is_expr_elem_                   = false; \
            is_expr_bulk_                   = false; \
            expr_bptr_                      = nullptr; \
            expr_info_1D_                   = new int[expr_past_info_1D_size_]; \
            expr_past_bulk_get_counter_    = 0; \
            expr_bulk_get_counter_         = 0; \
            expr_bulk_put_counter_         = 0; \
            GW_CLEAR_EXPR();\
        } \
        /* used for subarray construction for ndims > 1 */ \
        if ( ndims_ > 1 ) \
        { \
            subsizes_.reserve(ndims_); \
            starts_.reserve(ndims_); \
        } \
        /* for RMACXX_Subarray_t */ \
        lsizes_.reserve(ndims_); \
        lstarts_.reserve(ndims_); \
    } while(0)

    Window( std::vector<int> const& lo, std::vector<int> const& hi ) //HERE, constructor for window
    {
        comm_ = MPI_COMM_WORLD;
        is_comm_dupd_ = false;
        wkind_ = ALLOC;

#ifdef DEBUG_CHECK_GAPS //checks gaps where we want to place, all spaces should be allocated to a process
        std::vector<int> this_lo(lo.size()), this_hi(lo.size());
        for (int i = 0; i < lo.size(); i++) {
            //std::cout<<"BAM: "<<lo[i]<<" "<<hi[i]<<std::endl;
            this_lo[i] = lo[i];
            this_hi[i] = hi[i];
        }
        los.push_back(this_lo);
        his.push_back(this_hi);

#endif
        
        GWINIT_COMMON_RANGE( lo, hi );

#ifdef RMACXX_USE_SAME_SIZE_INFO

        if ( is_same_wsize_ )
        {
            MPI_Info_create( &winfo_ );
            MPI_Info_set( winfo_, "same_size", "true" );
        }

#endif
        /* Allocate window */
        T* base = nullptr;
        MPI_Win_allocate( nelems_ * sizeof( T ),
                          sizeof( T ), winfo_, comm_, &base, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
	    MPI_Barrier( comm_ );
        //by this point, all the processes have submitted their coordinates
        std::cout<<"los size: "<<los.size()<<std::endl;
        std::cout<<"los"<<std::endl;
        for (int i = 0; i < los[0].size(); i++) {
            std::cout<<los[0][i]<<std::endl;
        }
        std::cout<<"his"<<std::endl;
        for (int i = 0; i < his[0].size(); i++) {
            std::cout<<his[0][i]<<std::endl;
        }
    }
    
    Window( std::vector<int> const& lo, std::vector<int> const& hi, MPI_Comm comm )
    {
        MPI_Comm_dup( comm, &comm_ );
        is_comm_dupd_ = true;
        wkind_ = ALLOC;
        
        GWINIT_COMMON_RANGE( lo, hi );

#ifdef RMACXX_USE_SAME_SIZE_INFO

        if ( is_same_wsize_ )
        {
            MPI_Info_create( &winfo_ );
            MPI_Info_set( winfo_, "same_size", "true" );
        }

#endif
        /* Allocate window */
        T* base = nullptr;
        MPI_Win_allocate( nelems_ * sizeof( T ),
                          sizeof( T ), winfo_, comm_, &base, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
	MPI_Barrier( comm_ );
    }

    // add a member variable that stores whether a comm is dup'd
    Window( std::vector<int> const& lo, std::vector<int> const& hi, MPI_Comm comm, T* winbuf )
    {
        MPI_Comm_dup( comm, &comm_ );
        is_comm_dupd_ = true;
        wkind_ = CREATE;
        
        GWINIT_COMMON_RANGE( lo, hi );
#ifdef RMACXX_USE_SAME_SIZE_INFO

        if ( is_same_wsize_ )
        {
            MPI_Info_create( &winfo_ );
            MPI_Info_set( winfo_, "same_size", "true" );
        }

#endif
        MPI_Win_create( winbuf, nelems_ * sizeof( T ),
                        sizeof( T ), MPI_INFO_NULL, comm_, &win_ );
        MPI_Win_lock_all( MPI_MODE_NOCHECK, win_ );
        iswinlocked_ = true;
	MPI_Barrier( comm_ );
    }

    // user has to call wfree to deallocate
    // resources
    ~Window()
    {}

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

        if ( wuse_ == EXPR )
        {
            delete []expr_info_1D_;
            defer_xfer_nD_.clear();
            defer_put_xfer_1D_.clear();
            defer_put_xfer_nD_.clear();
            GW_CLEAR_EXPR();
        }

        expr_bptr_ = nullptr;
	MPI_Barrier( comm_ );
       
        if ( is_comm_dupd_ )
            MPI_Comm_free( &comm_ );
        if ( ndims_ > 1 )
            MPI_Comm_free( &cart_comm_ );
    }

    // Window attributes
    inline MPI_Comm comm() { return comm_; }
    inline MPI_Comm cart_comm() { return cart_comm_; }
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

    // some functions for Global Arrays compatibility

    // TODO FIXME this will be purged in the future 
    // NGA_Access
    void access( int lo[], int hi[], T** ptr, int ld[] )
    {
        T* base = wget();

        if ( ndims_ > 1 )
        {
            int dim = 0, idx = 0;	
            for ( int k = 0; k < ndims_; k++ )
            {
                int val = lo[k] - rlo_[commRank_*ndims_+k];
                for ( int i = ndims_-1; i >= dim+1; i-- )  
                    val *= dims_[i];

                idx += val;
                dim++;

                // ld
                if ( k > 1 )
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
        T* localBuf = wget();

        for ( int i = 0; i < nelems_; i++ )
            localBuf[i] *= (T)(2.0);

        MPI_Win_sync( win_ );
        MPI_Barrier( comm_ );
    }

    // copy window lo/hi ranges
    // TODO FIXME can we assume lo/hi are allocated?
    void wdim(std::vector<int> lo, std::vector<int> hi)
    { 
	memcpy( lo.data(), &rlo_[commRank_*ndims_], ndims_*sizeof(int) ); 
	memcpy( hi.data(), &rhi_[commRank_*ndims_], ndims_*sizeof(int) ); 
    }
    
    // copy window lo/hi ranges (1D)
    void wdim(int& lo, int& hi)
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
            int pdims = ndims_;
            // even for 1D data, process grid is 2D
            if (ndims_ == 1)
                pdims += 1;
            for ( int i = 0; i < pdims; i++ )
            {
                if (i == (pdims-1))
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
        bool found = false;

        for ( int p = 0; p < commSize_; p++ )
        {                
            if ( ndims_ > 1 )
            {
                int id = p*ndims_;

                for( auto itl = l.begin(); itl != l.end(); ++itl )
                {
                    if ( *itl >= rlo_[id] && *itl <= rhi_[id] )
                        found = true;
                    else
                    {
                        found = false;
                        break;
                    }

                    id++;
                }
            }
            else
            {
                auto lo = *( l.begin() );
                if ( lo  >= rlo_[p] && lo <= rhi_[p] )
                    found = true;
            }

            if ( found )
            {
                target_ = p;

                if ( ndims_ > 1 )
                {
                    disp_ = 0;
                    int idx = 0;
                    std::for_each( l.begin(), l.end(), [this, &idx] ( int val )
                    {
                        int local_val = val - rlo_[target_*ndims_+idx];

                        for ( int i = ndims_-1; i >= idx+1; i-- )
                            local_val *= dims_per_pe_[target_*ndims_+i];

                        disp_ += local_val;
                        idx++;
                    } );
                }
                else
                    disp_ = ( *l.begin() ) - rlo_[target_];
		
		break;
            }
        }
    }

    // returns target process owning an input coordinate
    // this is called repeatedly in bulk interface
    // this is called internally, so accepts a vector
    // instead of init_list
    inline int find_target( std::vector<int> const& l )
    {
        bool found = false;

        for ( int p = 0; p < commSize_; p++ )
        {
            if ( ndims_ > 1 )
            {
                int id = p*ndims_;

                for( auto itl = l.begin(); itl != l.end(); ++itl )
                {
                    if ( *itl >= rlo_[id] && *itl <= rhi_[id] )
                        found = true;
                    else
                    {
                        found = false;
                        break;
                    }

                    id++;
                }
            }
            else
            {
                auto lo = *( l.begin() );
                if ( ( lo  >= rlo_[p] ) && ( lo <= rhi_[p] ) )
                    found = true;
            }

            if ( found )
                return p;
        }
        
        return -1;
    }

    // returns target process owning an input coordinate
    // this is called repeatedly in bulk interface
    // update remote displacement and change name
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
    // Both l/h needs to be writable, as we update them...
    // this means extra time will be incurred in calling std::vector            HERE, the parentheses operator for a window
    // ctor which takes an init_list
    inline WIN& operator()( std::initializer_list<int> const& l,  std::initializer_list<int> const& h, X )
    {
        /*
        // run window check here
        std::vector<int> lv(l.size()), hv(h.size());
        lv.insert(lv.end(), l.begin(), l.end());
        hv.insert(hv.end(), h.begin(), h.end());

        for (int i = 0; i < l.size(); i++){
            if (lv[i] < lo_[i] || hv[i] > hi_[i] ) {
                // failed check
                std::cout << "Accessing out of the bounds of the window" << std::endl;
                abort();
            }
        }
        */
#ifdef DEBUG_CHECK_GAPS
        std::vector<int> l_vec; 
        l_vec.insert(l_vec.end(), l.begin(), l.end());  //(2,2,2,2,2,2,2)
        std::vector<int> h_vec; 
        h_vec.insert(h_vec.end(), h.begin(), h.end());  //(3,3,3,3,3,3,3)

        // l_vec and h_vec which are the start and end coords of the area we want to fill
        // have access to los and his, which are the start and end coordinates of all the processes
        // check to make sure all tiles between l_vec and h_vec are contained in los and his
        
        

#endif

    
#ifdef DEBUG_CHECK_LIMITS
        store_lo = l;
        store_hi = h;
#endif

        lock();

        if ( ndims_ > 1 )
        {
            // calculate dimensions of local buffer
            int idx = 0;
            auto ilo = l.begin();

            // other subarray params will be constructed during communicaton
            std::for_each( h.begin(), h.end(), [this, &ilo, &idx] ( int hi_val )
                    {
                    lo_[idx] = *ilo;
                    hi_[idx] = hi_val;
                    lsizes_[idx] = hi_[idx] - lo_[idx] + 1;
                    ilo++;
                    idx++;
                    } );
        }
        else
        {
            lo_[0] = *( l.begin() );
            hi_[0] = *( h.begin() );
            lsizes_[0] = hi_[0] - lo_[0] + 1;
        }

        return *this;
    }

    // elementwise, we calculate target, disp
    // in this function
    inline WIN& operator()( std::initializer_list<int> const& l, X )
    {
        lock();

        // target_, disp_ will be computed
        // in this function
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
        
        find_target_disp( l );

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
    inline void eexpr_outstanding_gets() const
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
    // for 1D cases involving expressions
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

    // bulk expression
    inline BExpr<T,RefBExpr<T,WIN>> operator()( std::initializer_list<int> const& l, std::initializer_list<int> const& h, Y )
    {
        lock();

        is_expr_bulk_ = true;
        lo_ = l; // this is (more than) a copy
        
        // this is to retain the previous count, in case
        // there is a window in the RHS, and current
        // parameters are no longer used for get
        expr_past_bulk_get_counter_ = expr_bulk_get_counter_;
        
        if ( ndims_ > 1 )
        {
            std::vector<int> new_hi( ndims_ ), new_lo( ndims_ );
//            int total_count = 0;
            new_lo = lo_;
            disp_ = 0;

            /* initial target */
            target_ = find_target( new_lo );
            
            while( 1 )
            {
                int i = 0;
                std::for_each( hi_.begin(), hi_.end(), [this, &new_hi, &new_lo, &i] ( int hi_val )
                        {
                        starts_[i] = new_lo[i] - rlo_[target_*ndims_+i];
                        if ( hi_val > rhi_[target_*ndims_+i] )
                            new_hi[i] = rhi_[target_*ndims_+i] - rlo_[target_*ndims_+i];
                        else
                            new_hi[i] = hi_val - rlo_[target_*ndims_+i];
                        subsizes_[i] = new_hi[i] - starts_[i] + 1;
                        /* for local buffer's subarray */
                        lstarts_[i] = new_lo[i] - lo_[i];
                        /* readjust new_hi to reflect global ID */
                        /* required for stopping criteria */
                        new_hi[i] += rlo_[target_*ndims_+i];
                        i++;
                        } );

                // insert into vector for later processing
                defer_xfer_nD_.emplace_back( target_, count_dimn_,
                                             subsizes_, starts_, lstarts_ );
                expr_bulk_get_counter_++;
                
                /* exit criteria */
                if ( std::equal( hi_.begin(), hi_.end(), new_hi.begin() ) )
                    break;
                
                /* update lo to point to next block and find next target */
                int npes = 0;
                int next_target = ( ( target_+1 ) == commSize_ ) ? 0 : ( target_+1 );
                
                while ( 1 )
                {
                    int matched = 0;
                    for( int k = 0; k < ndims_; k++ )
                    {
                        if ( std::abs( pe_coords_[next_target*ndims_+k] - pe_coords_[target_*ndims_+k] ) == 1 )
                            if ( hi_[k] >= rlo_[next_target*ndims_+k] && hi_[k] <= rhi_[next_target*ndims_+k] )
                                matched++;
                    }
                    if ( matched == ( ndims_-1 ) )
                    {
                        target_ = next_target;
                        
                        memcpy( new_lo.data(), &rlo_[target_*ndims_], ndims_*sizeof( int ) );
                        
                        for( int k = 0; k < ndims_; k++ )
                        {
                            if ( new_lo[k] < lo_[k] )
                                new_lo[k] = lo_[k];
                        }
                        break;
                    }
                    
                    next_target = ( ( next_target+1 ) == commSize_ ) ? 0 : ( next_target+1 );
                
                    /* this should not be executed, because we should find next target */
                    if ( npes == ( commSize_-1 ) )
                        break;
                    npes++;
                }
            }
        }
        else /* ndims == 1 */
        {
            int current_count = 0;
            const int total_count = hi_[0] - lo_[0] + 1;
            target_ = find_target( lo_ ); 
            /* disp will be lo only for the first */
            /* process, it will be 0 for the rest */
            disp_ = lo_[0] - rlo_[target_];
            count_dimn_ = rhi_[target_] - lo_[0] + 1;
            
            while( 1 )
            {
                if ( expr_issue_counter_ >= expr_past_info_1D_size_ )
                    resize_expr_info_1D_();

                expr_info_1D_[expr_issue_counter_] = target_;       /* target */
                expr_info_1D_[expr_issue_counter_+1] = count_dimn_; /* count */
                expr_info_1D_[expr_issue_counter_+2] = disp_;       /* disp */
                expr_issue_counter_ += 3;
                expr_bulk_get_counter_++;
                
                /* find next target, count */
                int next_target = ( ( target_+1 ) == commSize_ ) ? 0 : ( target_+1 );

                /* check if next target is next to me in process grid */
                while( 1 )
                {
                    if ( std::abs( pe_coords_[target_+1] - pe_coords_[next_target+1] ) == 1 )
                    {
                        target_ = next_target;
                        break;
                    }
                    next_target = ( ( next_target+1 ) == commSize_ ) ? 0 : ( next_target+1 );
                }
                current_count += count_dimn_;

                /* exit criteria */
                if ( current_count == total_count )
                    break;

                if ( rhi_[target_] > hi_[0] )
                    count_dimn_ = rhi_[target_] - hi_[0] + 1;
                else
                    count_dimn_ = rhi_[target_] - rlo_[target_] + 1;
                
                disp_ = 0;
            }
        }

        unlock();
        
        return BExpr<T,RefBExpr<T,WIN>>( RefBExpr<T,WIN>( *this ) );
    }

    // post outstanding gets for prior bulk
    // expression window access
    inline void bexpr_outstanding_gets() const
    {
        lock();

        for ( int k = 0; k < expr_bulk_get_counter_; k++ )
        {
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
                                MPI_Datatype tarr_type, sarr_type;
                                MPI_Type_create_subarray( ndims_, dims_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                          MPI_ORDER_C, TypeMap<T>(), &tarr_type );
                                MPI_Type_commit( &tarr_type );
                                MPI_Type_create_subarray( ndims_, dims_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].lstarts_.data(),
                                                          MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                                MPI_Type_commit( &sarr_type );
                                MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                                    &pbuf_.buf_.buffer_[expr_buf_counter_],
                                                    1, sarr_type, defer_xfer_nD_[expr_getND_counter_].target_,
                                                    /*disp*/ 0, 1, tarr_type, MPI_REPLACE, win_ );
                                MPI_Type_free( &tarr_type );
                                MPI_Type_free( &sarr_type );
                                expr_buf_counter_ += count;
                                expr_getND_counter_++;
                            }
                        }
                        else
                        {
                            MPI_Datatype tarr_type, sarr_type;
                            MPI_Type_create_subarray( ndims_, dims_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                      MPI_ORDER_C, TypeMap<T>(), &tarr_type );
                            MPI_Type_commit( &tarr_type );    
                            MPI_Type_create_subarray( ndims_, dims_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].lstarts_.data(),
                                                      MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                            MPI_Type_commit( &sarr_type );
                            MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL,
                                                &pbuf_.buf_.buffer_[expr_buf_counter_],
                                                1, sarr_type, defer_xfer_nD_[expr_getND_counter_].target_,
                                                /*disp*/ 0, 1, tarr_type, MPI_REPLACE, win_ );
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
                                MPI_Datatype tarr_type, sarr_type;
                                MPI_Type_create_subarray( ndims_, dims_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                          MPI_ORDER_C, TypeMap<T>(), &tarr_type );
                                MPI_Type_commit( &tarr_type );
                                MPI_Type_create_subarray( ndims_, dims_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                          defer_xfer_nD_[expr_getND_counter_].lstarts_.data(),
                                                          MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                                MPI_Type_commit( &sarr_type );
                                MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                                         1, sarr_type,
                                         defer_xfer_nD_[expr_getND_counter_].target_,/*disp*/ 0, 1,
                                         tarr_type, win_ );
                                MPI_Type_free( &sarr_type );
                                MPI_Type_free( &tarr_type );
                                expr_buf_counter_ += count;
                                expr_getND_counter_++;
                            }
                        }
                        else
                        {
                            MPI_Datatype sarr_type, tarr_type;
                            MPI_Type_create_subarray( ndims_, dims_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].starts_.data(),
                                                      MPI_ORDER_C, TypeMap<T>(), &tarr_type );
                            MPI_Type_commit( &tarr_type );
                            MPI_Type_create_subarray( ndims_, dims_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].subsizes_.data(),
                                                      defer_xfer_nD_[expr_getND_counter_].lstarts_.data(),
                                                      MPI_ORDER_C, TypeMap<T>(), &sarr_type );
                            MPI_Type_commit( &sarr_type );
                            MPI_Get( &pbuf_.buf_.buffer_[expr_buf_counter_],
                                     1, sarr_type,
                                     defer_xfer_nD_[expr_getND_counter_].target_,/*disp*/ 0, 1,
                                     tarr_type, win_ );
                            MPI_Type_free( &sarr_type );
                            MPI_Type_free( &tarr_type );
                            expr_buf_counter_ += count;
                            expr_getND_counter_++;
                        }
                    }
                }
            } // end of if ( expr_buf_counter_ < PREALLOC_BUF_SZ )
            else
                break; // no need to iterate if preallocated buffer is full
        } // end of for( k = 0 -> expr_bulk_get_counter_ )

        unlock();
    }

    // ignore get and adjust counters, since the
    // communication params are going to be used
    // put(s)
    inline void expr_ignore_last_get() const
    {
        lock();

        if ( ndims_ == 1 )
        {
            expr_issue_counter_ -= 3;

            if ( expr_info_1D_[expr_issue_counter_+1] > 1 ) // 1-D bulk
            {
                expr_bulk_put_counter_ = expr_bulk_get_counter_ - expr_past_bulk_get_counter_ + 1;
                expr_bulk_get_counter_ = expr_past_bulk_get_counter_;

                for ( int k = 0; k < expr_bulk_put_counter_; k++ )
                {
                    defer_put_xfer_1D_.emplace_back(
                        expr_info_1D_[expr_issue_counter_],
                        expr_info_1D_[expr_issue_counter_+1],
                        expr_info_1D_[expr_issue_counter_+2] );
                    expr_issue_counter_ -= 3;
                }
            }
            else // for n-D elementwise
            {
                defer_put_xfer_1D_.emplace_back(
                    expr_info_1D_[expr_issue_counter_],
                    expr_info_1D_[expr_issue_counter_+1],
                    expr_info_1D_[expr_issue_counter_+2] );
            }
        }
        else
        {
            expr_bulk_put_counter_ = expr_bulk_get_counter_ - expr_past_bulk_get_counter_ + 1;
            expr_bulk_get_counter_ = expr_past_bulk_get_counter_;

            for ( int k = 0; k < expr_bulk_put_counter_; k++ )
            {
                const int idx = defer_xfer_nD_.size()-1;
                defer_put_xfer_nD_.emplace_back( defer_xfer_nD_[idx].target_,
                                                 defer_xfer_nD_[idx].count_,
                                                 defer_xfer_nD_[idx].subsizes_,
                                                 defer_xfer_nD_[idx].starts_ );
                defer_xfer_nD_.pop_back();
            }
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

    // bulk put for expression cases with window in the RHS
    void bexpr_outstanding_put( const T* origin_addr ) const
    {
        lock();

        for ( int k = 0; k < expr_bulk_put_counter_; k++ )
        {
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
        }

        expr_bulk_put_counter_ = 0;
        unlock();
    }

    // for acc
    // -------
    inline WIN& operator()( std::initializer_list<int> const& l, std::initializer_list<int> const& h, Op op )
    {
        lock();
        winop_ = op;

        if ( ndims_ > 1 )
        { 
            // calculate dimensions of local buffer
            int idx = 0;
            auto ilo = l.begin();

            std::for_each( h.begin(), h.end(), [this, &ilo, &idx] ( int hi_val )
                    {
                    lo_[idx] = *ilo;
                    hi_[idx] = hi_val;
                    lsizes_[idx] = hi_[idx] - lo_[idx] + 1;
                    ilo++;
                    idx++;
                    } );
        }
        else
        {
            lo_[0] = *( l.begin() );
            hi_[0] = *( h.begin() );
            lsizes_[0] = hi_[0] - lo_[0] + 1;
        }

        return *this;
    }

    inline WIN& operator()( std::initializer_list<int> const& l, Op op )
    {
        lock();
        winop_ = op;
        
        // target_, disp_ will be computed
        // in this function
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
        fop_inp_ = const_cast<T*>( inpval );

        if ( ndims_ > 1 )
        { 
            // calculate dimensions of local buffer
            int idx = 0;
            auto ilo = l.begin();

            std::for_each( h.begin(), h.end(), [this, &ilo, &idx] ( int hi_val )
                    {
                    lo_[idx] = *ilo;
                    hi_[idx] = hi_val;
                    lsizes_[idx] = hi_[idx] - lo_[idx] + 1;
                    ilo++;
                    idx++;
                    } ); 
        }
        else
        {
            lo_[0] = *( l.begin() );
            hi_[0] = *( h.begin() );
            lsizes_[0] = hi_[0] - lo_[0] + 1;
        }

        return *this;
    }

    // for fetch-and-op
    // ----------------
    inline WIN& operator()( std::initializer_list<int> const& l, Op op, T inpval )
    {
        lock();
        winop_ = op;

        fop_inp_ = &inpval;
        is_fop_ = true;
        
        // target_, disp_ will be computed
        // in this function
        find_target_disp( l );
        
        return *this;
    }

    // for compare-and-swap
    // --------------------
    inline WIN& operator()( std::initializer_list<int> const& l, const T inpval, const T cmpval )
    {
        lock();
        is_cas_ = true;
        
        fop_inp_ = &inpval;
        cas_inp_ = &cmpval;
        
        // target_, disp_ will be computed
        // in this function
        find_target_disp( l );
        
        return *this;
    }

    // pushes+completes any pending deferred
    // elementwise gets
    T eval() const
    {
        T val = T( 0 );
        lock();
        int& target = expr_info_1D_[expr_xfer1D_counter_];

        if ( expr_size_counter_ < PREALLOC_BUF_SZ )
        {
            flush_local( target );
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

    // return the last element count for a window object
    // this is used to allocate memory for intermediate put
    // origin buffer in the bulk expression case with window
    // in the RHS
    inline int get_count() const { return count_dimn_; }

    // fillInto is used by bulk
    // expression interface for completing
    // all outstanding communication...
    // bexpr_outstanding_gets will use the
    // preallocated static buffer always...
    // so we need to move data from the static
    // buffer to the user pointer, or issue
    // communication operations on the
    // user pointer directly
    // ----------------------------------

    // returns the count of elements processed in get
    int fillInto( T* buf ) const
    {
        int count = 0, total_count = 0;
        lock();

        for ( int k = 0; k < expr_bulk_get_counter_; k++ )
        {
            if ( ndims_ > 1 )
            {
                // check if preallocated buffer was used in a previously
                // issued get
                count = defer_xfer_nD_[expr_xferND_counter_].count_;
                // TODO FIXME use subsizes and get the count of elements
                T* ptr = buf + k*count*sizeof( T );
                total_count += count;

                if ( expr_size_counter_ == 0 && ( ( expr_size_counter_ + count ) < PREALLOC_BUF_SZ ) )
                {
                    flush_local( defer_xfer_nD_[expr_xferND_counter_].target_ );
                    memcpy( ptr, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                    expr_size_counter_ += count;
                }
                else if ( expr_size_counter_ < PREALLOC_BUF_SZ )
                {
                    flush_local( defer_xfer_nD_[expr_xferND_counter_].target_ );
                    memcpy( ptr, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                    expr_size_counter_ += count;
                }
                else // issue+complete remaining gets
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
                        MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL, ptr,
                                            count, TypeMap<T>(), target,/*disp*/ 0, 1,
                                            sarr_type, MPI_REPLACE, win_ );
                    }
                    else
                    {
                        MPI_Get( ptr, count, TypeMap<T>(),
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
                T* ptr = buf + k*count*sizeof( T );
                total_count += count;

                if ( expr_size_counter_ == 0 && ( ( expr_size_counter_ + count ) < PREALLOC_BUF_SZ ) )
                {
                    flush_local( expr_info_1D_[expr_xfer1D_counter_] );
                    memcpy( ptr, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                    expr_size_counter_ += count;
                }
                else if ( expr_size_counter_ < PREALLOC_BUF_SZ )
                {
                    flush_local( expr_info_1D_[expr_xfer1D_counter_] );
                    expr_info_1D_[expr_xfer1D_counter_] = ( -1 )*expr_size_counter_;
                    memcpy( ptr, &pbuf_.buf_.buffer_[expr_size_counter_], sizeof( T )*count );
                    expr_size_counter_ += count;
                }
                else // issue+complete remaining gets
                {
                    int const& target = expr_info_1D_[expr_xfer1D_counter_];

                    if ( watmc_ == ATOMIC_PUT_GET )
                    {
                        MPI_Get_accumulate( NULL, 0, MPI_DATATYPE_NULL, ptr,
                                            count, TypeMap<T>(), target,
                                            expr_info_1D_[expr_xfer1D_counter_+2],
                                            count, TypeMap<T>(), MPI_REPLACE, win_ );
                    }
                    else
                    {
                        MPI_Get( ptr, count, TypeMap<T>(), target,
                                 expr_info_1D_[expr_xfer1D_counter_+2],
                                 count, TypeMap<T>(), win_ );
                    }

                    flush_local( target );
                }

                expr_xfer1D_counter_ += 3;
            } // end of ndims == 1
        }

        expr_bptr_ = buf;
        
        unlock();
        
        return total_count;
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
        GW_CLEAR_EXPR();
        
        expr_get_counter_               = 0;
        expr_getND_counter_             = 0;
        expr_xfer1D_counter_            = 0;
        expr_xferND_counter_            = 0;
        expr_size_counter_              = 0;
        expr_buf_counter_               = 0;
        expr_put_eval_counter_          = 0;
        expr_issue_counter_             = 0;
        expr_info_1D_counter_           = 0;
        defer_xfer_nD_counter_          = 0;
        
        expr_past_info_1D_size_         = ( 3*DEFAULT_EXPR_COUNT );
        expr_bptr_                      = nullptr;
        is_expr_elem_                   = false;
        is_expr_bulk_                   = false;
        expr_bulk_get_counter_          = 0;
        expr_past_bulk_get_counter_     = 0;
    }

    /* Bulk operations */
    /* --------------- */
    
    // operators overloading for standard
    // bulk put/get
    // -------------------------------------
    void operator <<( const T* origin_addr )
    {
        if ( winop_.op == MPI_OP_NULL )
            RMACXX_GLOBAL_CONTIG_BULK_XFER(origin_addr, RMACXX_BULK_PUT_GLOBAL);
        else
	{
            RMACXX_GLOBAL_CONTIG_BULK_XFER(origin_addr, RMACXX_BULK_ACC_GLOBAL);
            winop_ = Op();
	}

        if ( wcmpl_ == LOCAL_FLUSH )
            flush_local();

        if ( wcmpl_ == REMOTE_FLUSH )
            flush();


        unlock();
    }
   
    // overloading ostream op for bulk `get
    // ------------------------------------
    void operator >>( T* origin_addr )
    {
        if ( is_fop_ )
	{
            RMACXX_GLOBAL_CONTIG_BULK_XFER(origin_addr, RMACXX_BULK_GACC_GLOBAL);
            is_fop_ = false;
	}
        else
            RMACXX_GLOBAL_CONTIG_BULK_XFER(origin_addr, RMACXX_BULK_GET_GLOBAL);

        if ( wcmpl_ == LOCAL_FLUSH )
            flush_local();

        if ( wcmpl_ == REMOTE_FLUSH )
            flush();

        unlock();
    }

    // operator overloading for put/get
    // with user-defined types
    // NC stands for non-contiguous
    
    // TODO FIXME we rely on the user to *not* use
    // RMACXX_Subarray_t with elementwise operations.
    // At present, no compile time error is thrown,
    // is there a way to fix it?

    // overload istream op for bulk noncontiguous `put/acc
    // ---------------------------------------------------
    
    void operator <<( RMACXX_Subarray_t<T, GLOBAL_VIEW> const& origin )
    {
        //HERE, the << operator for  Subarray
        // check to make sure the size of the subarray matches up with the window
        // store_lo and store_hi are inclusive coordinates on the Window

        //std::vector<int> nlo, nhi;

#ifdef RMACXX_SUBARRAY_USE_END_COORDINATES
        // if we're given inclusive coordinates
        std::vector<int> nhi;
        nhi.insert(nhi.end(), store_hi.begin(), store_hi.end());
#ifdef DEBUG_CHECK_LIMITS
        if (origin.sizes_ != nhi) {
            //failed check
            std::cout << "Coordinates not equal" << std::endl;
            abort();
        }
#endif
#else
        //if we're given size
        std::vector<int> nlo, nhi;
        nlo.insert(nlo.end(), store_lo.begin(), store_lo.end());
        nhi.insert(nhi.end(), store_hi.begin(), store_hi.end());

        //take all the hi and lo values and convert them into sizes
        std::vector<int> sizes(nhi.size());
        for (int i = 0; i < nlo.size(); i++) {
            sizes[i] = nhi[i] - nlo[i] + 1;
        }
#ifdef DEBUG_CHECK_LIMITS
        if (origin.sizes_ != sizes) {
            //failed check
            std::cout << "Coordinates not equal" << std::endl;
            abort();
        }
#endif
#endif

        if ( winop_.op == MPI_OP_NULL )
            RMACXX_GLOBAL_BULK_XFER_NC(origin, RMACXX_BULK_PUT_GLOBAL);
        else
        {
            RMACXX_GLOBAL_BULK_XFER_NC(origin, RMACXX_BULK_ACC_GLOBAL);
            winop_ = Op();
	    }

        if ( wcmpl_ == LOCAL_FLUSH )
            flush_local();

        if ( wcmpl_ == REMOTE_FLUSH )
            flush();

        unlock();
    }

    // overloading ostream op for bulk noncontiguous `get
    // --------------------------------------------------
    void operator >>( RMACXX_Subarray_t<T, GLOBAL_VIEW> const& origin )
    {
        // check subarray and window sizes
        //this.

        if ( is_fop_ )
        {
            RMACXX_GLOBAL_BULK_XFER_NC(origin, RMACXX_BULK_GACC_GLOBAL);
            is_fop_ = false;
	    }
        else
            RMACXX_GLOBAL_BULK_XFER_NC(origin, RMACXX_BULK_GET_GLOBAL);

        if ( wcmpl_ == LOCAL_FLUSH )
            flush_local();

        if ( wcmpl_ == REMOTE_FLUSH )
            flush();

        unlock();
    }

    /* Elementwise operations */
    /* ---------------------- */
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
            RMACXX_FOP( val );
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
            GWFLUSH();
        }
    }

    inline void block_on_expr(exprid expr) const { this->expressions_.emplace_back(expr); }
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
            GWFLUSH();
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
        std::vector<int> lstarts_;
        ExprDefer_nD( int target, int count,
                      std::vector<int> subsizes,
                      std::vector<int> starts,
                      std::vector<int> lstarts
                    ):
            target_( target ), count_( count ),
            subsizes_( subsizes ), starts_( starts ),
            lstarts_( lstarts )
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
        std::vector<int> subsizes_, sizes_;
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

    MPI_Comm comm_, cart_comm_;
    int commRank_;
    int commSize_;

    mutable MPI_Win win_;
    bool iswinlocked_, is_same_wsize_, is_pgrid_unidir_;
    int pgrid_unidx_;
    // Win kind, win_create or win_alloc'd
    WinKind wkind_;
    Op winop_;
    MPI_Info winfo_;

    int ndims_;
    std::vector<int> dims_, dims_per_pe_; // dims per PE
    std::vector<int> pe_coords_, pgrid_;
    int nelems_;

    // target and disp_ populated
    // in operator()() for elementwise
    int target_;
    MPI_Aint disp_;

    // for bulk interface, we compute
    // size, ranks on the fly in op>>
    // or op<<
    std::vector<int> lo_;
    std::vector<int> hi_;

    // stores data range per process
    std::vector<int> rlo_, rhi_;


    // stores the data range from the subarray constructor   HERE
    std::initializer_list<int> store_lo;
    std::initializer_list<int> store_hi;

    


    // stores previous value at
    // a particular offset, used
    // by get_last() function
    T* fop_inp_;
    T* cas_inp_;
    bool is_fop_, is_cas_, is_comm_dupd_;

#ifndef RMACXX_USE_CLASSIC_HANDLES
    mutable std::vector<exprid> expressions_;
#endif
    // parameters for subarray construction
    mutable std::vector<int> subsizes_, starts_;
    mutable std::vector<int> lsizes_, lstarts_;
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
            expr_put_eval_counter_,
            expr_info_1D_counter_,
            defer_xfer_nD_counter_,
            expr_bulk_get_counter_,
            expr_bulk_put_counter_,
            expr_past_bulk_get_counter_;

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
}; // class Window<T,GLOBAL_VIEW,...>
