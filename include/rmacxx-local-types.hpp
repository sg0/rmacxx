// Handle subarray types in origin buffer
//
// An RMACXX_Subarray_t describes a strided origin buffer: an n-d (C-ordered)
// array of extents `sizes_` in which the patch starts at `starts_`. With the
// NVSHMEM backend the patch is packed/unpacked on the host and staged to the
// GPU (no MPI derived datatypes are involved).

// Local view (default)
// -------------------

template <typename T>
struct RMACXX_Subarray_t<T, LOCAL_VIEW>
{
    // ctor that also takes subsizes (kept for compatibility; the patch
    // extents are taken from the window access)
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts,
            std::initializer_list<int> const& subsizes,
            std::initializer_list<int> const& sizes )
    {
        starts_.insert( starts_.end(), starts.begin(), starts.end() );
        subsizes_.insert( subsizes_.end(), subsizes.begin(), subsizes.end() );
        sizes_.insert( sizes_.end(), sizes.begin(), sizes.end() );
        ptr_ = nullptr;
    }

    RMACXX_Subarray_t ( std::initializer_list<int> const& starts,
            std::initializer_list<int> const& sizes )
    {
        starts_.insert( starts_.end(), starts.begin(), starts.end() );
        sizes_.insert( sizes_.end(), sizes.begin(), sizes.end() );
        ptr_ = nullptr;
    }

    // only stores starts (for 1D)
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts )
    {
        starts_.insert( starts_.end(), starts.begin(), starts.end() );
        ptr_ = nullptr;
    }

    RMACXX_Subarray_t ( RMACXX_Subarray_t const& subarr_t ) = default;

    ~RMACXX_Subarray_t()
    {
        starts_.clear();
        sizes_.clear();
        subsizes_.clear();
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

    std::vector<int> sizes_, starts_, subsizes_;
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
        ptr_ = nullptr;\
    } while(0)

#define RMACXX_SUBARRAY_STORE_GLOBAL_1D(starts) \
    do { \
        starts_.insert(starts_.end(), starts.begin(), starts.end()); \
        if (starts_.size() > 1) \
        { \
            std::cout << "RMACXX_Subarray_t: need sizes for >1D, use the other ctor" << std::endl; \
            RMACXX_FATAL( "RMACXX_Subarray_t: need sizes for >1D" ); \
        } \
        ptr_ = nullptr; \
    } while(0)

#ifdef RMACXX_SUBARRAY_USE_END_COORDINATES // pass the starts and ends constructors into the macro, then perform conversions HERE
    RMACXX_Subarray_t ( std::initializer_list<int> const& starts,
            std::initializer_list<int> const& ends )
    {
        RMACXX_SUBARRAY_STORE_GLOBAL(ends, starts);  // ends, starts -> sizes_, starts_
        for (size_t i = 0; i < sizes_.size(); i++) {
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

    RMACXX_Subarray_t ( RMACXX_Subarray_t const& subarr_t ) = default;

    ~RMACXX_Subarray_t()
    {
        sizes_.clear();
        starts_.clear();
    }

    std::vector<int> sizes_, starts_;
    T* ptr_;
};
