SRCS       := $(wildcard *.cpp)
PRGS       := $(patsubst %.cpp,%,$(SRCS))
PRG_SUFFIX  =.exe
BINS       := $(patsubst %,%$(PRG_SUFFIX),$(PRGS))
OBJS       := $(patsubst %,%.o,$(PRGS))
# TEST_OVERHEAD will use MPI_PROC_NULL (no communication)
CXXFLAGS = -O3 -Wall -std=c++11 -pthread -I../src -D_GNU_SOURCE -DTEST_OVERHEAD
LDFLAGS  =
MPICXX 	 =  mpicxx
RUNCMD 	 =  mpiexec
NPROCS	 = 2
CXX      = $(MPICXX)

.PHONY: all clean run

all : $(BINS)

OBJ = $(patsubst %$(PRG_SUFFIX),%.o,$@)
BIN = $@

%$(PRG_SUFFIX) : $(OBJS)
	$(MPICXX) $(CXXFLAGS) $(OBJ) -o $(BIN) $(LDFLAGS)

run :
	for bin in $(BINS); do \
		$(RUNCMD) -np $(NPROCS) ./$$bin; \
	done

clean:
	$(RM) $(BINS) $(OBJS)
