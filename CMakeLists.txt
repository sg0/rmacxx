cmake_minimum_required (VERSION 3.8)

# set the project name
project(rmacxx)

message (STATUS "Adding mpiService")

# find_package(MPI REQUIRED)

set(CMAKE_C_COMPILER mpicc)
set(CMAKE_CXX_COMPILER mpicxx)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")


# set(MPI_GUESS_LIBRARY_NAME MPICH2)

message(STATUS ${MPI_INCLUDE_PATH})
message(STATUS ${MPI_C_LIBRARIES})

add_library(rmacxx INTERFACE) #add_library(RMACXX INTERFACE include/)
target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_SOURCE_DIR}/include/)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_20)

enable_testing()
include(CTest)

set(TEST_DIR tests)
set(BENCH_DIR benchmarks)

file(GLOB Level1 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*/*.cpp)
file(GLOB Level2 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*/*/*.cpp)
file(GLOB Benches RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${BENCH_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${BENCH_DIR}/*.cpp)
set(TestsToRun ${Level1} ${Level2})
function(get_test_info path_var without_ext_var test_name_var num_processes_var default_num)
  cmake_path(REMOVE_EXTENSION ${path_var} LAST_ONLY OUTPUT_VARIABLE without_ext) # Remove cpp
  cmake_path(GET without_ext EXTENSION num_from_ext) # Get potential number
  if(num_from_ext STREQUAL "")
    set(new_path ${without_ext})
    set(num_processes ${default_num})
  else()              # optional block
    string(SUBSTRING ${num_from_ext} 1 -1 num_from_ext_no_period)
    cmake_path(REMOVE_EXTENSION ${path_var} OUTPUT_VARIABLE new_path)
    cmake_path(REMOVE_EXTENSION num_from_ext_no_period OUTPUT_VARIABLE num_processes)
  endif()
  string(REPLACE "/"
    "." new_path
    ${new_path})
  set(${test_name_var} ${new_path} PARENT_SCOPE)
  set(${without_ext_var} ${without_ext} PARENT_SCOPE)
  set(${num_processes_var} ${num_processes} PARENT_SCOPE)
endfunction()



foreach (test ${Benches})
  get_test_info(test no_ext test_name test_processes 10)
  set(executable_name bench_${test_name}.${test_processes})
  add_executable(${executable_name} ${PROJECT_SOURCE_DIR}/${BENCH_DIR}/${test})
  target_compile_features(${executable_name} PRIVATE cxx_std_20)
  target_include_directories(${executable_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  add_test (NAME ${executable_name} COMMAND mpiexec --np ${test_processes} ${PROJECT_BINARY_DIR}/${executable_name})
endforeach ()


# Add all the ADD_TEST for each test
foreach (test ${TestsToRun})
  get_test_info(test no_ext test_name test_processes 2)
  set(executable_name ${test_name}.${test_processes})
  add_executable(${executable_name} ${PROJECT_SOURCE_DIR}/${TEST_DIR}/${test})
  target_compile_features(${executable_name} PRIVATE cxx_std_20)
  target_include_directories(${executable_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  add_test (NAME ${executable_name} COMMAND mpiexec --np ${test_processes} ${PROJECT_BINARY_DIR}/${executable_name})
endforeach ()

