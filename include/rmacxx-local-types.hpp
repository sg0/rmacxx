// Handle subarray types in origin buffer

// Local view (default)
// -------------------

// There are two ctors, one that creates a subarr 
// type right away, and the other one stores 
// starts/sizes, and later computes subsizes from
// lo/hi passed to the window object.
template <typename T> 
struct RMACXX_Subarray_t<T, LOCAL_VIEW>
{
// converts initalizer lists into vectors
#define RMACXX_SUBARRAY_TYPE_CREATE(starts, subsizes, sizes) \
    do { \
        starts_.insert(starts_.end(), starts.begin(), starts.end()); \
        if (starts_.size() > 1) \
        { \
            std::vector<int> lsubsizes; \
            sizes_.insert(sizes_.end(), sizes.begin(), sizes.end()); \
            lsubsizes.insert(lsubsizes.end(), subsizes.begin(), subsizes.end()); \
            MPI_Type_create_subarray(sizes_.size(), sizes_.data(), lsubsizes.data(), \
                    starts_.data(), MPI_ORDER_C, TypeMap<T>(), &dtype_); \
            MPI_Type_commit(&dtype_); \
        } \
        else \
        { \
            dtype_ = MPI_DATATYPE_NULL; \
        } \
        ptr_ = nullptr; \
    } while(0)

#define RMACXX_SUBARRAY_STORE(starts, sizes) \
    do { \
        starts_.insert(starts_.end(), starts.begin(), starts.end()); \
        sizes_.insert(sizes_.end(), sizes.begin(), sizes.end()); \
        dtype_ = MPI_DATATYPE_NULL; \
        /* to be filled later */ \
        ptr_ = nullptr; \
    } while(0)

#define RMACXX_SUBARRAY_STORE_1D(starts) \
    do { \
        /* 1D case, sizes not needed */ \
        starts_.insert(starts_.end(), starts.begin(), starts.end()); \
        /* ignore */ \
        dtype_ = MPI_DATATYPE_NULL; \
        /* to be filled later */ \
        ptr_ = nullptr; \
    } while(0)

    // ctor that takes subsizes, and creates a datatype
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts, 
            std::initializer_list<int> const& subsizes, 
            std::initializer_list<int> const& sizes )
    { RMACXX_SUBARRAY_TYPE_CREATE(starts, subsizes, sizes); }
   
    // ctor that does not create a datatype
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts, 
            std::initializer_list<int> const& sizes )
    { RMACXX_SUBARRAY_STORE(starts, sizes); }
    
    // ctor that does not create a datatype and only 
    // stores starts (for 1D)
    // TODO FIXME don't use vector to store one data point, use
    // an int
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts )
    { RMACXX_SUBARRAY_STORE_1D(starts); }
    
    RMACXX_Subarray_t ( RMACXX_Subarray_t const& subarr_t )
    {
        starts_ = subarr_t.starts_;
        sizes_ = subarr_t.sizes_;

        if (dtype_ != MPI_DATATYPE_NULL)
            MPI_Type_dup(subarr_t.dtype, &dtype_); 
    }
    
    ~RMACXX_Subarray_t() 
    {
        starts_.clear();
        sizes_.clear();

        if (dtype_ != MPI_DATATYPE_NULL)
            MPI_Type_free(&dtype_); 
    }

    inline RMACXX_Subarray_t& operator()( T* ptr )
    {
        ptr_ = ptr;
        return *this;
    }

    inline RMACXX_Subarray_t& operator()( const T* ptr )
    {
        ptr_ = const_cast<T*>( ptr );
        return *this;
    }

    MPI_Datatype dtype_;
    std::vector<int> sizes_, starts_;
    T* ptr_;
};

// Global view
// -----------

// There is no benefit in accepting subsizes
// for global view, as depending on the shape
// of the target patch, the origin subsizes
// will change.

template <typename T> 
struct RMACXX_Subarray_t<T, GLOBAL_VIEW>
{
// macro converts intializer lists into vectors
#define RMACXX_SUBARRAY_STORE_GLOBAL(sizes, starts) \
    do { \
        sizes_.insert(sizes_.end(), sizes.begin(), sizes.end()); \
        starts_.insert(starts_.end(), starts.begin(), starts.end()); \
        ptr_ = nullptr; std::cout<<"we were here"<<std::endl; \
    } while(0)
 
#define RMACXX_SUBARRAY_STORE_GLOBAL_1D(starts) \
    do { \
        starts_.insert(starts_.end(), starts.begin(), starts.end()); \
        if (starts_.size() > 1) \
        { \
            std::cout << "RMACXX_Subarray_t: need sizes for >1D, use the other ctor" << std::endl; \
            MPI_Abort(99, MPI_COMM_WORLD); \
        } \
        ptr_ = nullptr; \
    } while(0)

#ifdef RMACXX_SUBARRAY_USE_END_COORDINATES // pass the starts and ends constructors into the macro, then perform conversions HERE
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts, 
            std::initializer_list<int> const& ends ) 
    {
        RMACXX_SUBARRAY_STORE_GLOBAL(ends, starts);  // ends, starts -> sizes_, starts_
        for (int i = 0; i < sizes_.size(); i++) {
            sizes_[i] = sizes_[i] - starts_[i] + 1; // we do +1 because ends is inclusive
        }
    }
#else // pass the starts and sizes constructors into the macro, default
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts, 
            std::initializer_list<int> const& sizes )
    { RMACXX_SUBARRAY_STORE_GLOBAL(sizes, starts); }
#endif
    
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts )
    { RMACXX_SUBARRAY_STORE_GLOBAL_1D( starts ); }
    
    inline RMACXX_Subarray_t& operator()( T* ptr )
    {
        ptr_ = ptr;
        return *this;
    }

    inline RMACXX_Subarray_t& operator()( const T* ptr )
    {
        ptr_ = const_cast<T*>( ptr );
        return *this;
    }   
    
    RMACXX_Subarray_t ( RMACXX_Subarray_t const& subarr_t )
    {
        starts_ = subarr_t.starts_;
        sizes_ = subarr_t.sizes_;
    }
    
    ~RMACXX_Subarray_t() 
    {
        sizes_.clear();
        starts_.clear();
    }

    std::vector<int> sizes_, starts_;
    T* ptr_;
};
