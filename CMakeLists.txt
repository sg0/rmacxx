cmake_minimum_required (VERSION 3.17)

# set the project name
project(rmacxx)

message (STATUS "Adding mpiService")

# find_package(MPI REQUIRED)

set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpicxx)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++17 -pthread")


# set(MPI_GUESS_LIBRARY_NAME MPICH2)

message(STATUS ${MPI_INCLUDE_PATH})
message(STATUS ${MPI_C_LIBRARIES})

add_library(rmacxx INTERFACE) #add_library(RMACXX INTERFACE include/)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_SOURCE_DIR}/include/)
# target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

enable_testing()
include(CTest)

set(TEST_DIR tests)
set(BENCH_DIR benchmarks)

file(GLOB Benches RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${BENCH_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${BENCH_DIR}/*.cpp)
foreach (test ${Benches})
  set(executable_name bench_${test})
  add_executable(${executable_name} ${PROJECT_SOURCE_DIR}/${BENCH_DIR}/${test})
  # target_compile_features(${executable_name} PRIVATE cxx_std_20)
  target_include_directories(${executable_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  add_test (NAME ${executable_name} COMMAND mpiexec --np 20 ${PROJECT_BINARY_DIR}/${executable_name})
endforeach ()
