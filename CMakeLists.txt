cmake_minimum_required (VERSION 3.8)

# set the project name
project(rmacxx)

message (STATUS "Adding mpiService")

find_package(MPI REQUIRED)

set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpicxx)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(MPI_GUESS_LIBRARY_NAME MPICH2)

message(STATUS ${MPI_INCLUDE_PATH})
message(STATUS ${MPI_C_LIBRARIES})

add_library(rmacxx INTERFACE) #add_library(RMACXX INTERFACE include/)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_SOURCE_DIR}/include/)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

add_subdirectory(${CMAKE_SOURCE_DIR}/tests)