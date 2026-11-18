SRCS       := $(wildcard */*.cpp */*/*.cpp)
SRCS       := $(filter-out emulator/%,$(SRCS))
PRGS       := $(patsubst %.cpp,%,$(SRCS))
PRG_SUFFIX  =.test
BINS       := $(patsubst %,%$(PRG_SUFFIX),$(PRGS))

# GPU build: CUDA + NVSHMEM (MPI bootstrap, launched with mpiexec)
NVSHMEM_HOME ?= /usr/local/nvshmem
ARCH         ?= sm_80
NVCC          = nvcc -x cu -rdc=true -ccbin $(MPICXX)
CXXFLAGS      = -O3 -std=c++20 -arch=$(ARCH) -I../include -I$(NVSHMEM_HOME)/include -D_GNU_SOURCE -DDEBUG_CHECK_GAPS
LDFLAGS       = -L$(NVSHMEM_HOME)/lib -lnvshmem_host -lnvshmem_device -lcuda
MPICXX 	      = mpicxx
RUNCMD 	      = mpiexec
NPROCS	      = 2

.PHONY: build check clean emu

build: $(BINS)

show:
	echo "$(BINS)"

BIN = $@

%$(PRG_SUFFIX): %.cpp
	echo "::group::$(BIN)"
	($(NVCC) $(CXXFLAGS) $^ -o $(BIN) $(LDFLAGS)||(echo "::endgroup::"; exit 2));
	($(RUNCMD) -np $(or $(subst .,,$(suffix $(basename $^))),$(NPROCS)) ./$(BIN)||(echo "::endgroup::";exit 2));
	echo "::endgroup::"

check: $(BINS)

# CPU emulator (no GPU / NVSHMEM / MPI needed)
emu:
	./emulator/run_tests.sh

clean:
	$(RM) */*$(PRG_SUFFIX) */*/*$(PRG_SUFFIX)
