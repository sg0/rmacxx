SRCS       := $(wildcard */*/*.cpp)
# SRCS       := $(wildcard */*.cpp */*/*.cpp)
PRGS       := $(patsubst %.cpp,%,$(SRCS))
PRG_SUFFIX  =.test
OBJ_SUFFIX  =.o
BINS       := $(patsubst %,%$(PRG_SUFFIX),$(PRGS))
OBJS       := $(patsubst %,%$(OBJ_SUFFIX),$(PRGS))

CXXFLAGS = -O3 -Wall -std=c++20 -pthread -I../include -D_GNU_SOURCE -DTEST_OVERHEAD -DDEBUG_CHECK_GAPS
LDFLAGS  =
MPICXX 	 =  mpic++
RUNCMD 	 =  mpiexec
NPROCS	 = 2
CXX      = $(MPICXX)

.PHONY: build check run_tests

build: $(OBJS)

show: 
	echo "$(OBJS)"

BIN = $@

%$(PRG_SUFFIX): %$(OBJ_SUFFIX)
	echo "::group::$(BIN)"
	($(MPICXX) $(CXXFLAGS) $^ -o $(BIN) $(LDFLAGS)||(echo "::endgroup::"; exit 2));
	($(RUNCMD) -np $(or $(subst .,,$(suffix $(basename $^))),$(NPROCS)) ./$(BIN)||(echo "::endgroup::";exit 2));
	echo "::endgroup::"


check: $(OBJS) $(BINS)

clean:
	$(RM) */*$(PRG_SUFFIX) */*/*$(PRG_SUFFIX) */*$(OBJ_SUFFIX) */*/*$(OBJ_SUFFIX)
