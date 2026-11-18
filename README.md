# RMACXX

![](https://img.shields.io/badge/C%2B%2B%20Version-C++20-informational)
![Test Status](https://github.com/sg0/rmacxx/actions/workflows/main.yml/badge.svg?branch=main)

RMACXX is a C++ global/local-view interface for one-sided communication:
users create a distributed n-dimensional array (irregular per-process
blocks are fine) and access patches of it with n-dimensional arithmetic
coordinates; optional expression templates evaluate expressions over
remote patches.

RMACXX was originally a thin layer over MPI-3 RMA. **It now uses NVSHMEM**:
window memory lives in GPU memory (the NVSHMEM symmetric heap), and the same
patch operations can be initiated from the host *or directly from CUDA
kernels* (device-initiated communication).

Paper on RMACXX:

Sayan Ghosh, Yanfei Guo, Pavan Balaji, Assefaw Gebremedhin. 
RMACXX: An Efficient High-Level C++ Interface over MPI-3 RMA. 
21st IEEE/ACM International Symposium on Cluster, Cloud and 
Internet Computing (CCGrid 2021).

## Requirements

* CUDA 12+ (C++20 device code), a GPU per process
* NVSHMEM 2.x or 3.x (built with MPI bootstrap support for the default setup)
* MPI, used only to launch processes and bootstrap NVSHMEM (`mpiexec`). Build
  with `-DRMACXX_USE_MPI_BOOTSTRAP=0` to use NVSHMEM's own bootstrap
  (`nvshmrun`) instead.

Translation units that include `rmacxx.hpp` must be compiled as CUDA with
relocatable device code and linked against NVSHMEM, e.g.

```
nvcc -x cu -rdc=true -ccbin mpicxx -std=c++20 -arch=sm_90 -I include -I $NVSHMEM_HOME/include \
     app.cpp -L $NVSHMEM_HOME/lib -lnvshmem_host -lnvshmem_device -lcuda -o app
mpiexec -np 4 ./app
```

With CMake, link against the `rmacxx` interface target (it pulls in NVSHMEM,
CUDA and MPI) and set `CUDA_SEPARABLE_COMPILATION ON`:

```
cmake --preset default -DNVSHMEM_HOME=/path/to/nvshmem -DCMAKE_CUDA_ARCHITECTURES=90
cmake --build --preset default && ctest --preset default
```

## Host API (unchanged)

```c++
rmacxx::init(&argc, &argv);              // instead of MPI_Init (MPI + GPU selection + NVSHMEM)

rmacxx::Window<double, GLOBAL_VIEW> win(lo, hi);   // this PE's block [lo, hi]
win.fill(1.0);
win({2,4,1},{7,8,8}) << buf;             // put a patch spanning several PEs
win({2,4,1},{7,8,8}, SUM) << buf;        // accumulate
win({2,4,1},{7,8,8}) >> buf;             // get
win.flush();

rmacxx::Window<int, LOCAL_VIEW, EXPR> ew({10});
ew(1,{0}) + 2*ew(1,{1}) >> x;            // expression templates (placement-new handles)
ew.flush();

win.wfree();
rmacxx::finalize();                      // instead of MPI_Finalize
```

All operators, completion modes (`NO_FLUSH`, `LOCAL_FLUSH`, `REMOTE_FLUSH`),
`ATOMIC_PUT_GET`, `CONCURRENT`, `RMACXX_Subarray_t` strided origins and the
element-wise/bulk expression templates work as before.

## Device-initiated communication

`Window::device_view()` returns a trivially copyable `rmacxx::DeviceWindow`
that kernels use directly:

```c++
__global__ void kernel(rmacxx::DeviceWindow<double, GLOBAL_VIEW> dw, const double* src, double* dst)
{
    // whole block cooperates on a patch that may span several PEs
    dw.put<rmacxx::Scope::BLOCK>({0, 0}, {1, 7}, src);         // blocking (quiet inside)
    dw.get_nbi<rmacxx::Scope::WARP>({2, 0}, {3, 7}, dst);      // non-blocking
    if (threadIdx.x == 0) {
        dw.acc({7, 7}, 1.0, SUM);                              // atomic accumulate
        double old = dw.fetch_op({0, 0}, 2.0, MAX);
        double prev = dw.cas({0, 1}, 0.0, 42.0);
    }
    dw.quiet();
}

kernel<<<1, 256, 0, win.stream()>>>(win.device_view(), d_src, d_dst);
win.barrier();   // completes device-initiated operations everywhere
```

* Local view: `dw.put(target, lo, hi, src)` etc., element access with
  `dw.put(target, {i, j}, v)` / `dw.get(target, {i, j})`.
* `Scope::THREAD` (default), `Scope::WARP`, `Scope::BLOCK`; group scopes are
  collective (all threads of the warp/block call with the same arguments).
* `*_nbi` calls complete after `quiet()`; calls without `_nbi` are blocking.
* Strided origin buffers: `rmacxx::Origin<T>(ptr, dims, starts)`.
* `dw.ptr(pe)` gives a load/store pointer for NVLink-connected PEs.
* Over network transports, origin buffers passed to device calls must be
  symmetric (`nvshmem_malloc`) or registered (`rmacxx::register_buffer`).
  Element-wise calls pass values in registers and have no such requirement.

See `tests/nvshmem/device-api.4.cpp` for a complete example.

## How it maps to NVSHMEM

* Window buffers are allocated on the symmetric heap. NVSHMEM allocations are
  symmetric, so every PE allocates the largest per-PE block (size the heap with
  `NVSHMEM_SYMMETRIC_SIZE` if needed).
* Every access is described by a patch descriptor (an n-d box in the target's
  local array and the matching box in the origin). Global-view patches are
  split by intersecting them with every PE's block. Contiguous dimensions are
  merged, so a contiguous patch is a single transfer.
* Put/get move rows with `nvshmem[x]_{put,get}mem_nbi[_warp|_block]`.
  Accumulate, get-accumulate, fetch-and-op, compare-and-swap and
  `ATOMIC_PUT_GET` are element-wise NVSHMEM AMOs: native fetch-add/swap where
  NVSHMEM has them, a compare-and-swap loop for other operators and for
  floating point, and a CAS on the enclosing 32-bit word for 1- and 2-byte
  types.
* Host operations run as kernels on the window's stream (`win.stream()`);
  host origin buffers are staged through a registered device buffer, so they
  are reusable as soon as an operation returns.

## Differences from the MPI RMA version

* Windows always span all PEs (symmetric allocation is collective over
  `NVSHMEM_TEAM_WORLD`). Constructors taking an `MPI_Comm` remain, but the
  communicator must be congruent with `MPI_COMM_WORLD`.
* `wget()` (and GA-style `access()`) return a **device** pointer.
* Constructors taking a user buffer (`MPI_Win_create` model) initialize the
  window from it and copy the window back into it in `wfree()`; NVSHMEM cannot
  expose arbitrary user memory remotely.
* NVSHMEM has no per-target completion: `flush(target)` and
  `flush_local(target)` complete everything.
* Results of host gets (and get-accumulate / fetch-and-op / CAS) are delivered
  before `operator>>` returns. Define `RMACXX_DEFER_HOST_GETS` to deliver them
  at the next flush, as MPI RMA allows.
* `wlock()`/`wunlock()` are no-ops returning `void` (there are no epochs).
* `MAXLOC`/`MINLOC` are not supported; accumulate operators require 1/2/4/8
  byte arithmetic types (other types support put/get and `REPLACE`).
* Expression accesses now remember which window access they refer to, so
  gets, evaluation and puts no longer depend on operand evaluation order.

## Testing without GPUs

`tests/emulator` contains a CPU emulation of the CUDA/NVSHMEM/MPI subset used
by RMACXX (one PE per thread, one symmetric-heap arena per PE). It runs the
same library code and kernels, and is what CI uses:

```
cmake --preset emulator && cmake --build --preset emulator && ctest --preset emulator
# or
tests/emulator/run_tests.sh [tests/expr-elem/expr-elem.cpp ...]
```

It checks indexing, patch decomposition, staging, expressions and AMO logic;
it does not model GPU performance or the GPU memory model.

`tests/dimensions/simple_gv_bulk_3D_subarray.4.cpp` fails: its
`RMACXX_Subarray_t` describes an origin array (`sizes`) that does not contain
the requested patch (`starts + extent > sizes`), which
`MPI_Type_create_subarray` also rejects.

The benchmarks in `benchmarks/` still compare against raw MPI RMA and have
not been ported.
