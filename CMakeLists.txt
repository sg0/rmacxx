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
target_include_directories(rmacxx INTERFACE include/)

target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

file(GLOB SOURCE_FILES ${CMAKE_SOURCE_DIR}/tests/*/*.cpp)

enable_testing()
include(CTest)

create_test_sourcelist (Tests CommonCxxTests.cpp ${SOURCE_FILES})

# add the executable
add_executable (CommonCxxTests ${Tests})

# remove the test driver source file
set (TestsToRun ${Tests})
remove (TestsToRun CommonCxxTests.cxx)

# Add all the ADD_TEST for each test
foreach (test ${TestsToRun})
  get_filename_component (TName ${test} NAME_WE)
  add_test (NAME ${TName} COMMAND CommonCxxTests ${TName})
endforeach ()