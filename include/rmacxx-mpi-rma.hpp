/* Bulk RMA operations */
/*---------------------*/
// acc
// ---
#define RMACXX_BULK_ACC(origin_addr) \
                                    do { \
                                        if (ndims_ == 1) \
                                        MPI_Accumulate(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                disp_, count_dimn_, TypeMap<T>(), winop_.op, win_); \
                                        else \
                                        { \
                                            MPI_Datatype sarr_type; \
                                            MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                    starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                            MPI_Type_commit(&sarr_type); \
                                            MPI_Accumulate(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    /*disp*/ 0, 1, sarr_type, winop_.op, win_); \
                                            MPI_Type_free(&sarr_type); \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        winop_ = Op(); \
                                        unlock(); \
                                    } while(0)

#define RMACXX_BULK_ACC_GLOBAL(origin_addr) \
                                    do { \
                                        if (ndims_ == 1) \
                                        MPI_Accumulate(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                disp_, count_dimn_, TypeMap<T>(), winop_.op, win_); \
                                        else \
                                        { \
                                                MPI_Datatype target_sarr_type, source_sarr_type; \
                                                MPI_Type_create_subarray(ndims_, &dims_per_pe_[target_*ndims_], subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &target_sarr_type); \
                                                MPI_Type_commit(&target_sarr_type); \
                                                MPI_Type_create_subarray(ndims_, lsizes_.data(), subsizes_.data(), \
                                                        lstarts_.data(), MPI_ORDER_C, TypeMap<T>(), &source_sarr_type); \
                                                MPI_Type_commit(&source_sarr_type); \
                                                MPI_Accumulate(origin_addr, 1, source_sarr_type, target_, \
                                                        /*disp*/ 0, 1, target_sarr_type, winop_.op, win_); \
                                                MPI_Type_free(&target_sarr_type); \
                                                MPI_Type_free(&source_sarr_type); \
                                        } \
                                    } while(0)

// non-contiguous acc
// ------------------
#define RMACXX_BULK_ACC_NC(origin) \
                                    do { \
                                        if (ndims_ == 1) \
                                        MPI_Accumulate(&origin.ptr_[origin.starts_[0]], 1, TypeMap<T>(), target_, \
                                                disp_, count_dimn_, TypeMap<T>(), winop_.op, win_); \
                                        else \
                                        { \
                                            if ( origin.dtype_ == MPI_DATATYPE_NULL ) \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Type_create_subarray(ndims_, origin.sizes_.data(), subsizes_.data(), \
                                                        origin.starts_.data(), MPI_ORDER_C, TypeMap<T>(), &origin.dtype_); \
                                                MPI_Type_commit(&origin.dtype_); \
                                                MPI_Accumulate(origin.ptr_, 1, origin.dtype_, target_, \
                                                        /*disp*/ 0, 1, sarr_type, winop_.op, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Accumulate(origin.ptr_, 1, origin.dtype_, target_, \
                                                        /*disp*/ 0, 1, sarr_type, winop_.op, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        winop_ = Op(); \
                                        unlock(); \
                                    } while(0)

// -------------- //

// put
// ---
// local view cases
// ---
#define RMACXX_BULK_PUT(origin_addr) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Accumulate(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), MPI_REPLACE, win_); \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Accumulate(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                        /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        else \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Put(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), win_); \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Put(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                        /*disp*/ 0, 1, sarr_type, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        unlock(); \
                                    } while(0)

// ---
// Global view cases
// ---

// RMACXX_BULK_<PUT|GET|ACC|GACC>_GLOBAL macros 
// are not to be called directly, and are used 
// within RMACXX_GLOBAL_CONTIG_BULK_XFER
// and RMACXX_GLOBAL_BULK_XFER_NC 

#define RMACXX_BULK_PUT_GLOBAL(origin_addr) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        { \
                                            if (ndims_ == 1) \
                                                MPI_Accumulate(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), MPI_REPLACE, win_); \
                                            else \
                                            { \
                                                MPI_Datatype target_sarr_type, source_sarr_type; \
                                                MPI_Type_create_subarray(ndims_, &dims_per_pe_[target_*ndims_], subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &target_sarr_type); \
                                                MPI_Type_commit(&target_sarr_type); \
                                                MPI_Type_create_subarray(ndims_, lsizes_.data(), subsizes_.data(), \
                                                        lstarts_.data(), MPI_ORDER_C, TypeMap<T>(), &source_sarr_type); \
                                                MPI_Type_commit(&source_sarr_type); \
                                                MPI_Accumulate(origin_addr, 1, source_sarr_type, target_, \
                                                        /*disp*/ 0, 1, target_sarr_type, MPI_REPLACE, win_); \
                                                MPI_Type_free(&target_sarr_type); \
                                                MPI_Type_free(&source_sarr_type); \
                                            } \
                                        } \
                                        else \
                                        { \
                                            if (ndims_ == 1) \
                                                MPI_Put(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), win_); \
                                            else \
                                            { \
                                                MPI_Datatype target_sarr_type, source_sarr_type; \
                                                MPI_Type_create_subarray(ndims_, &dims_per_pe_[target_*ndims_], subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &target_sarr_type); \
                                                MPI_Type_commit(&target_sarr_type);  \
                                                MPI_Type_create_subarray(ndims_, lsizes_.data(), subsizes_.data(), \
                                                        lstarts_.data(), MPI_ORDER_C, TypeMap<T>(), &source_sarr_type); \
                                                MPI_Type_commit(&source_sarr_type); \
                                                MPI_Put(origin_addr, 1, source_sarr_type, target_, \
                                                        /*disp*/ 0, 1, target_sarr_type, win_); \
                                                MPI_Type_free(&target_sarr_type); \
                                                MPI_Type_free(&source_sarr_type); \
                                            } \
                                        } \
                                    } while(0)

// put from a non-contiguous
// buffer
// -------------------------
#define RMACXX_BULK_PUT_NC(origin) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Accumulate(&origin.ptr_[origin.starts_[0]], count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), MPI_REPLACE, win_); \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                if ( origin.dtype_ == MPI_DATATYPE_NULL ) \
                                                { \
                                                    MPI_Type_create_subarray(ndims_, origin.sizes_.data(), subsizes_.data(), \
                                                            origin.starts_.data(), MPI_ORDER_C, TypeMap<T>(), \
                                                            &origin.dtype_); \
                                                    MPI_Type_commit( &origin.dtype_ ); \
                                                } \
                                                MPI_Accumulate(origin.ptr_, count_dimn_, origin.dtype_, target_, \
                                                        /*disp*/disp_, count_dimn_, sarr_type, MPI_REPLACE, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        else \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Put(&origin.ptr_[origin.starts_[0]], count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), win_); \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                if ( origin.dtype_ == MPI_DATATYPE_NULL ) \
                                                { \
                                                    MPI_Type_create_subarray(ndims_, origin.sizes_.data(), subsizes_.data(), \
                                                            origin.starts_.data(), MPI_ORDER_C, TypeMap<T>(), &origin.dtype_ ); \
                                                    MPI_Type_commit( &origin.dtype_ ); \
                                                } \
                                                MPI_Put(origin.ptr_, count_dimn_, origin.dtype_, target_, \
                                                        /*disp*/ disp_, count_dimn_, sarr_type, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        unlock(); \
                                    } while(0)

// -------------- //

// get
// ---
#define RMACXX_BULK_GET(origin_addr) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, origin_addr, \
                                                    count_dimn_, TypeMap<T>(), target_, disp_, \
                                                    count_dimn_, TypeMap<T>(), MPI_REPLACE, win_); \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, origin_addr, \
                                                        count_dimn_, TypeMap<T>(), target_, \
                                                        /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        else \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Get(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), win_); \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Get(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                        /*disp*/ 0, 1, sarr_type, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        unlock(); \
                                    } while(0)

#define RMACXX_BULK_GET_GLOBAL(origin_addr) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, origin_addr, \
                                                    count_dimn_, TypeMap<T>(), target_, disp_, \
                                                    count_dimn_, TypeMap<T>(), MPI_REPLACE, win_); \
                                            else \
                                            { \
                                                MPI_Datatype target_sarr_type, source_sarr_type; \
                                                MPI_Type_create_subarray(ndims_, &dims_per_pe_[target_*ndims_], subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &target_sarr_type); \
                                                MPI_Type_commit(&target_sarr_type); \
                                                MPI_Type_create_subarray(ndims_, lsizes_.data(), subsizes_.data(), \
                                                        lstarts_.data(), MPI_ORDER_C, TypeMap<T>(), &source_sarr_type); \
                                                MPI_Type_commit(&source_sarr_type); \
                                                MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, origin_addr, \
                                                        1, source_sarr_type, target_, \
                                                        /*disp*/ 0, 1, target_sarr_type, MPI_REPLACE, win_); \
                                                MPI_Type_free(&target_sarr_type); \
                                                MPI_Type_free(&source_sarr_type); \
                                            } \
                                        } \
                                        else \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Get(origin_addr, count_dimn_, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), win_); \
                                            else \
                                            { \
                                                MPI_Datatype target_sarr_type, source_sarr_type; \
                                                MPI_Type_create_subarray(ndims_, &dims_per_pe_[target_*ndims_], subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &target_sarr_type); \
                                                MPI_Type_commit(&target_sarr_type); \
                                                MPI_Type_create_subarray(ndims_, lsizes_.data(), subsizes_.data(), \
                                                        lstarts_.data(), MPI_ORDER_C, TypeMap<T>(), &source_sarr_type); \
                                                MPI_Type_commit(&source_sarr_type); \
                                                MPI_Get(origin_addr, 1, source_sarr_type, target_, \
                                                        /*disp*/ 0, 1, target_sarr_type, win_); \
                                                MPI_Type_free(&target_sarr_type); \
                                                MPI_Type_free(&source_sarr_type); \
                                            } \
                                        } \
                                    } while(0)

// get-accumulate
// --------------
#define RMACXX_BULK_GACC(origin_addr) \
                                    do { \
                                        if (ndims_ == 1) \
                                            MPI_Get_accumulate(fop_inp_, count_dimn_, TypeMap<T>(), origin_addr, \
                                                count_dimn_, TypeMap<T>(), target_, disp_, \
                                                count_dimn_, TypeMap<T>(), winop_.op, win_); \
                                        else \
                                        { \
                                            MPI_Datatype sarr_type; \
                                            MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                    starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                            MPI_Type_commit(&sarr_type); \
                                            MPI_Get_accumulate(fop_inp_, count_dimn_, TypeMap<T>(), origin_addr, \
                                                    count_dimn_, TypeMap<T>(), target_, \
                                                    /*disp*/ 0, 1, sarr_type, winop_.op, win_); \
                                            MPI_Type_free(&sarr_type); \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        is_fop_ = false; \
                                        winop_ = Op(); \
                                        unlock(); \
                                    } while(0)


#define RMACXX_BULK_GACC_GLOBAL(origin_addr) \
                                    do { \
                                        if (ndims_ == 1) \
                                            MPI_Get_accumulate(fop_inp_, count_dimn_, TypeMap<T>(), origin_addr, \
                                                count_dimn_, TypeMap<T>(), target_, disp_, \
                                                count_dimn_, TypeMap<T>(), winop_.op, win_); \
                                        else \
                                        { \
                                            MPI_Datatype target_sarr_type, source_sarr_type; \
                                            MPI_Type_create_subarray(ndims_, &dims_per_pe_[target_*ndims_], subsizes_.data(), \
                                                    starts_.data(), MPI_ORDER_C, TypeMap<T>(), &target_sarr_type); \
                                            MPI_Type_commit(&target_sarr_type); \
                                            MPI_Type_create_subarray(ndims_, lsizes_.data(), subsizes_.data(), \
                                                    lstarts_.data(), MPI_ORDER_C, TypeMap<T>(), &source_sarr_type); \
                                            MPI_Type_commit(&source_sarr_type); \
                                            MPI_Get_accumulate(fop_inp_, 1, source_sarr_type, origin_addr, \
                                                    1, source_sarr_type, target_, \
                                                    /*disp*/ 0, 1, target_sarr_type, winop_.op, win_); \
                                            MPI_Type_free(&target_sarr_type); \
                                            MPI_Type_free(&source_sarr_type); \
                                        } \
                                    } while(0)

// get into a non-contiguous
// buffer
// -------------------------
#define RMACXX_BULK_GET_NC(origin) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, &origin.ptr_[origin.starts_[0]], \
                                                    1, TypeMap<T>(), target_, disp_, \
                                                    count_dimn_, TypeMap<T>(), MPI_REPLACE, win_); \
                                            else \
                                            { \
                                                if ( origin.dtype_ == MPI_DATATYPE_NULL ) \
                                                { \
                                                    MPI_Datatype sarr_type; \
                                                    MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                            starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                    MPI_Type_commit(&sarr_type); \
                                                    MPI_Type_create_subarray(ndims_, origin.sizes_.data(), subsizes_.data(), \
                                                            origin.starts_.data(), MPI_ORDER_C, TypeMap<T>(), &origin.dtype_); \
                                                    MPI_Type_commit(&origin.dtype_); \
                                                    MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, origin.ptr_, \
                                                            1, origin.dtype_, target_, \
                                                            /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_); \
                                                    MPI_Type_free(&sarr_type); \
                                                } \
                                                else \
                                                { \
                                                    MPI_Datatype sarr_type; \
                                                    MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                            starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                    MPI_Type_commit(&sarr_type); \
                                                    MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, origin.ptr_, \
                                                            1, origin.dtype_, target_, \
                                                            /*disp*/ 0, 1, sarr_type, MPI_REPLACE, win_); \
                                                    MPI_Type_free(&sarr_type); \
                                                } \
                                            } \
                                        } \
                                        else \
                                        { \
                                            if (ndims_ == 1) \
                                            MPI_Get(&origin.ptr_[origin.starts_[0]], 1, TypeMap<T>(), target_, \
                                                    disp_, count_dimn_, TypeMap<T>(), win_); \
                                            else \
                                            { \
                                                if ( origin.dtype_ == MPI_DATATYPE_NULL ) \
                                                { \
                                                    MPI_Datatype sarr_type; \
                                                    MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                            starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                    MPI_Type_commit(&sarr_type); \
                                                    MPI_Type_create_subarray(ndims_, origin.sizes_.data(), subsizes_.data(), \
                                                            origin.starts_.data(), MPI_ORDER_C, TypeMap<T>(), &origin.dtype_); \
                                                    MPI_Type_commit(&origin.dtype_); \
                                                    MPI_Get(origin.ptr_, 1, origin.dtype_, target_, \
                                                            /*disp*/ 0, 1, sarr_type, win_); \
                                                    MPI_Type_free(&sarr_type); \
                                                } \
                                                else \
                                                { \
                                                    MPI_Datatype sarr_type; \
                                                    MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                            starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                    MPI_Type_commit(&sarr_type); \
                                                    MPI_Get(origin.ptr_, 1, origin.dtype_, target_, \
                                                            /*disp*/ 0, 1, sarr_type, win_); \
                                                    MPI_Type_free(&sarr_type); \
                                                } \
                                            } \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        unlock(); \
                                    } while(0)


// get-accumulate into a
// noncontoguous buffer
// ---------------------
#define RMACXX_BULK_GACC_NC(origin) \
                                    do { \
                                        if (ndims_ == 1) \
                                        MPI_Get_accumulate(fop_inp_, count_dimn_, TypeMap<T>(), &origin.ptr_[origin.starts_[0]], \
                                                1, TypeMap<T>(), target_, disp_, \
                                                count_dimn_, TypeMap<T>(), winop_.op, win_); \
                                        else \
                                        { \
                                            if ( origin.dtype_ == MPI_DATATYPE_NULL ) \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Type_create_subarray(ndims_, origin.sizes_.data(), subsizes_.data(), \
                                                        origin.starts_.data(), MPI_ORDER_C, TypeMap<T>(), &origin.dtype_); \
                                                MPI_Type_commit(&origin.dtype_); \
                                                MPI_Get_accumulate(fop_inp_, count_dimn_, TypeMap<T>(), origin.ptr_, \
                                                        1, origin.dtype_, target_, \
                                                        /*disp*/ 0, 1, sarr_type, winop_.op, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                            else \
                                            { \
                                                MPI_Datatype sarr_type; \
                                                MPI_Type_create_subarray(ndims_, dims_.data(), subsizes_.data(), \
                                                        starts_.data(), MPI_ORDER_C, TypeMap<T>(), &sarr_type); \
                                                MPI_Type_commit(&sarr_type); \
                                                MPI_Get_accumulate(fop_inp_, count_dimn_, TypeMap<T>(), origin.ptr_, \
                                                        1, origin.dtype_, target_, \
                                                        /*disp*/ 0, 1, sarr_type, winop_.op, win_); \
                                                MPI_Type_free(&sarr_type); \
                                            } \
                                        } \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        is_fop_ = false; \
                                        winop_ = Op(); \
                                        unlock(); \
                                    } while(0)

/* Element-wise RMA operations */
/*-----------------------------*/
// acc
// ---
#define RMACXX_ELEM_ACC(val) \
                                    do { \
                                        MPI_Accumulate(&val, 1, TypeMap<T>(), target_, \
                                                disp_, 1, TypeMap<T>(), winop_.op, win_); \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        winop_ = Op(); \
                                        unlock(); \
                                    } while(0)

// -------------- //

// put
// ---
#define RMACXX_ELEM_PUT(val) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        MPI_Accumulate(&val, 1, TypeMap<T>(), target_, \
                                                disp_, 1, TypeMap<T>(), MPI_REPLACE, win_); \
                                        else \
                                        MPI_Put(&val, 1, TypeMap<T>(), target_, \
                                                disp_, 1, TypeMap<T>(), win_); \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        unlock(); \
                                    } while(0)

// -------------- //

// get
// ---
#define RMACXX_ELEM_GET(val) \
                                    do { \
                                        if (watmc_ == ATOMIC_PUT_GET) \
                                        MPI_Get_accumulate(NULL, 0, MPI_DATATYPE_NULL, &val, \
                                                1, TypeMap<T>(), target_, disp_, \
                                                1, TypeMap<T>(), MPI_REPLACE, win_); \
                                        else \
                                        MPI_Get(&val, 1, TypeMap<T>(), target_, \
                                                disp_, 1, TypeMap<T>(), win_); \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        unlock(); \
                                    } while(0)

// -------------- //

// fetch-and-op
// ------------
#define RMACXX_FOP_ALT(val) \
                                    do { \
                                        MPI_Fetch_and_op(fop_inp_, &val, TypeMap<T>(), \
                                                target_, disp_, winop_.op, win_); \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        is_fop_ = false; \
                                        unlock(); \
                                    } while(0)


#define RMACXX_FOP(val) \
                                    do { \
                                        MPI_Get_accumulate(fop_inp_, 1, TypeMap<T>(), &val, 1, TypeMap<T>(), \
                                                target_, disp_, 1, TypeMap<T>(), winop_.op, win_); \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        is_fop_ = false; \
                                        unlock(); \
                                    } while(0)

// compare-and-swap
// ----------------
#define RMACXX_CAS(val) \
                                    do { \
                                        /* reusing fop_inp ptr for CAS */ \
                                        MPI_Compare_and_swap(fop_inp_, cas_inp_, &val, TypeMap<T>(), \
                                                target_, disp_, win_); \
                                        if (wcmpl_ == LOCAL_FLUSH) \
                                        flush_local(target_); \
                                        if (wcmpl_ == REMOTE_FLUSH) \
                                        flush(target_); \
                                        is_cas_ = false; \
                                        unlock(); \
                                    } while(0)

// ------------------------------------- //
/* Global view bulk transfer operations */
// ------------------------------------- //

// TODO FIXME bypass datatype creation for an element

// contiguous transfer
// -------------------

#define RMACXX_GLOBAL_CONTIG_BULK_XFER(origin_addr, RMACXX_OP) \
    do { \
        if ( ndims_ > 1 ) \
        { \
            std::vector<int> new_hi( ndims_ ), new_lo( ndims_ ); \
            memcpy( new_lo.data(), lo_.data(), ndims_*sizeof(int) ); \
            /* initial target */ \
            target_ = find_target( new_lo ); \
            disp_ = 0; \
            while( 1 ) \
            { \
                for ( int i = 0; i < ndims_; i++ ) \
                { \
                    starts_[i] = new_lo[i] - rlo_[target_*ndims_+i]; \
                    if ( hi_[i] > rhi_[target_*ndims_+i] ) \
                        new_hi[i] = rhi_[target_*ndims_+i] - rlo_[target_*ndims_+i]; \
                    else \
                        new_hi[i] = hi_[i] - rlo_[target_*ndims_+i]; \
                    /* lsizes_ computed by the window instance */ \
                    subsizes_[i] = new_hi[i] - starts_[i] + 1; \
                    /* for local buffer's subarray, subsizes is common */ \
                    lstarts_[i] = new_lo[i] - lo_[i]; \
                    /* readjust new_hi to reflect global ID */ \
                    /* required for stopping criteria */ \
                    new_hi[i] += rlo_[target_*ndims_+i]; \
                } \
                /* we have all subarray params, call MPI */ \
                RMACXX_OP( origin_addr ); \
                /* exit criteria */ \
                if ( memcmp( new_hi.data(), hi_.data(), sizeof(new_hi.data()) ) == 0 ) \
                    break; \
                /* update lo to point to next block and find next target */ \
                /* this can never be equal to commSize_ */\
                int next_target = target_ + 1; \
                while ( 1 ) \
                { \
                    if ( memcmp( hi_.data(), &rlo_[next_target*ndims_], sizeof(hi_.data()) ) >= 0 ) \
                    { \
                        for( int k = 0; k < ndims_; k++ ) \
                        { \
                            int const& lo = rlo_[next_target*ndims_+k]; \
                            if ( lo_[k] > lo) \
                                new_lo[k] = lo_[k]; \
                            else \
                                new_lo[k] = lo; \
                        } \
                        target_ = next_target; \
                        break; \
                    } \
                    /* next process in grid */\
                    next_target += 1; \
                } \
            } \
        } \
        else /* ndims == 1 */ \
        { \
            /* disp_ set only for the starting \
             * process in find_target, it will 
             * be 0 for the rest 
             */ \
            target_ = find_target( lo_[0] ); \
            /* start count */\
            count_dimn_ = rhi_[target_] - lo_[0] + 1; \
            int current_index = 0; \
            while( 1 ) \
            { \
                RMACXX_OP( &origin_addr[current_index] ); \
                current_index += count_dimn_; \
                /* exit criteria */ \
                if ( current_index == lsizes_[0] ) \
                    break; \
                target_ += 1; \
                if ( rhi_[target_] > hi_[0] ) \
                    count_dimn_ = hi_[0] - rlo_[target_] + 1; \
                else \
                    count_dimn_ = rhi_[target_] - rlo_[target_] + 1; \
                disp_ = 0; \
            } \
        } \
    } while(0)

// noncontiguous transfer
// ----------------------

/* Origin contains starts_, subsizes_ and sizes_ for local buffer */

/* Main difference between RMACXX_OP and RMACXX_OP_GV is that 
 * the latter has to undergo an extra subarray type creation
 * for the origin, whereas the former does not. In local view 
 * cases, the origin (subarray) datatype can be constructed in 
 * advance. This is not possible for global view cases, as 
 * given an arbitrary lo/hi range, one needs to find all the 
 * intermediate data ranges spread across the processes.
 */

/* TODO FIXME there is very little difference 
 * between this and the one above, consider 
 * combining them...
 */ 
#define RMACXX_GLOBAL_BULK_XFER_NC(origin, RMACXX_OP_GV) \
    do { \
        if ( ndims_ > 1 ) \
        { \
            /* initial target */ \
            std::vector<int> new_hi( ndims_ ), new_lo( ndims_ ); \
            memcpy( new_lo.data(), lo_.data(), ndims_*sizeof(int) ); \
            /* override lsizes as computed from lo/hi,
             * with stride-per-dim that user passed */ \
            memcpy( lsizes_.data(), origin.sizes_.data(), ndims_*sizeof(int) ); \
            disp_ = 0; \
            target_ = find_target( new_lo ); \
            while( 1 ) \
            { \
                for ( int i = 0; i < ndims_; i++ ) \
                { \
                    starts_[i] = new_lo[i] - rlo_[target_*ndims_+i]; \
                    if ( hi_[i] > rhi_[target_*ndims_+i] ) \
                        new_hi[i] = rhi_[target_*ndims_+i] - rlo_[target_*ndims_+i]; \
                    else \
                        new_hi[i] = hi_[i] - rlo_[target_*ndims_+i]; \
                    subsizes_[i] = new_hi[i] - starts_[i] + 1; \
                    /* for local buffer's subarray, subsizes is common */ \
                    lstarts_[i] = new_lo[i] - lo_[i] + origin.starts_[i]; \
                    /* readjust new_hi to reflect global ID */ \
                    /* required for stopping criteria */ \
                    new_hi[i] += rlo_[target_*ndims_+i]; \
                } \
                /* we have all subarray params for current slice, call MPI */ \
                RMACXX_OP_GV( origin.ptr_ ); \
                /* exit criteria */ \
                if ( memcmp( new_hi.data(), hi_.data(), sizeof(new_hi.data()) ) == 0 ) \
                    break; \
                /* update lo to point to next block and find next target */ \
                /* this can never be equal to commSize_ */\
                int next_target = target_ + 1; \
                while ( 1 ) \
                { \
                    if ( memcmp( hi_.data(), &rlo_[next_target*ndims_], sizeof(hi_.data()) ) >= 0 ) \
                    { \
                        for( int k = 0; k < ndims_; k++ ) \
                        { \
                            int const& lo = rlo_[next_target*ndims_+k]; \
                            if ( lo_[k] > lo) \
                                new_lo[k] = lo_[k]; \
                            else \
                                new_lo[k] = lo; \
                        } \
                        target_ = next_target; \
                        break; \
                    } \
                    /* next process in grid */\
                    next_target += 1; \
                } \
            } \
        } \
        else /* ndims == 1, 1d layout of PE grid */ \
        { \
            /* disp_ set only for the starting \
             * process in find_target, it will 
             * be 0 for the rest 
             */ \
            target_ = find_target( lo_[0] ); \
            /* start count */\
            count_dimn_ = rhi_[target_] - lo_[0] + 1; \
            int current_count = 0; \
            while( 1 ) \
            { \
                T* ptr = const_cast<T*>( origin.ptr_ ) + (origin.starts_[0] + current_count)*sizeof(T); \
                RMACXX_OP_GV( ptr ); \
                current_count += count_dimn_; \
                /* exit criteria */ \
                if ( current_count == lsizes_[0] ) \
                    break; \
                target_ += 1; \
                if ( rhi_[target_] > hi_[0] ) \
                    count_dimn_ = hi_[0] - rlo_[target_] + 1; \
                else \
                    count_dimn_ = rhi_[target_] - rlo_[target_] + 1; \
                disp_ = 0; \
            } \
        } \
    } while(0)
