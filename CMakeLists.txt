cmake_minimum_required (VERSION 3.8)
project(rmacxx)
message (STATUS "Adding mpiService")

find_package(MPI REQUIRED)

set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpicxx)
#set(CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
set(MPI_GUESS_LIBRARY_NAME MPICH2)

message(STATUS ${MPI_INCLUDE_PATH})
message(STATUS ${MPI_C_LIBRARIES})

#add_definitions(-DOMPI_SKIP_MPICXX)

add_library(rmacxx INTERFACE)
target_include_directories(rmacxx INTERFACE include/)