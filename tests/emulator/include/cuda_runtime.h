// CPU emulation of the CUDA runtime subset used by RMACXX (testing only)
#pragma once
#include "emu_core.hpp"

typedef int cudaError_t;
enum : int { cudaSuccess = 0, cudaErrorInvalidValue = 1, cudaErrorMemoryAllocation = 2 };
typedef void* cudaStream_t;

enum cudaMemcpyKind
{
    cudaMemcpyHostToHost = 0,
    cudaMemcpyHostToDevice = 1,
    cudaMemcpyDeviceToHost = 2,
    cudaMemcpyDeviceToDevice = 3,
    cudaMemcpyDefault = 4
};

enum cudaMemoryType
{
    cudaMemoryTypeUnregistered = 0,
    cudaMemoryTypeHost = 1,
    cudaMemoryTypeDevice = 2,
    cudaMemoryTypeManaged = 3
};

struct cudaPointerAttributes
{
    cudaMemoryType type;
    int device;
    void* devicePointer;
    void* hostPointer;
};

#define cudaStreamNonBlocking 0x01

inline const char* cudaGetErrorString( cudaError_t e ) { return e == cudaSuccess ? "no error" : "emulated CUDA error"; }
inline cudaError_t cudaGetLastError() { return cudaSuccess; }
inline cudaError_t cudaGetDeviceCount( int* n ) { *n = 1; return cudaSuccess; }
inline cudaError_t cudaSetDevice( int ) { return cudaSuccess; }
inline cudaError_t cudaDeviceSynchronize() { return cudaSuccess; }

template <typename P>
inline cudaError_t cudaMalloc( P** p, size_t bytes )
{
    *p = static_cast<P*>( std::aligned_alloc( 256, ( bytes + 255 ) & ~size_t( 255 ) ) );
    return *p ? cudaSuccess : cudaErrorMemoryAllocation;
}
inline cudaError_t cudaFree( void* p ) { std::free( p ); return cudaSuccess; }
inline cudaError_t cudaMemcpy( void* d, const void* s, size_t n, cudaMemcpyKind ) { std::memmove( d, s, n ); return cudaSuccess; }
inline cudaError_t cudaMemcpyAsync( void* d, const void* s, size_t n, cudaMemcpyKind, cudaStream_t ) { std::memmove( d, s, n ); return cudaSuccess; }
inline cudaError_t cudaMemset( void* d, int v, size_t n ) { std::memset( d, v, n ); return cudaSuccess; }
inline cudaError_t cudaStreamCreateWithFlags( cudaStream_t* s, unsigned ) { *s = reinterpret_cast<void*>( new int( 0 ) ); return cudaSuccess; }
inline cudaError_t cudaStreamDestroy( cudaStream_t s ) { delete reinterpret_cast<int*>( s ); return cudaSuccess; }
inline cudaError_t cudaStreamSynchronize( cudaStream_t ) { return cudaSuccess; }
inline cudaError_t cudaPointerGetAttributes( cudaPointerAttributes* a, const void* )
{
    a->type = cudaMemoryTypeUnregistered;
    a->device = 0;
    a->devicePointer = nullptr;
    a->hostPointer = nullptr;
    return cudaSuccess;
}
