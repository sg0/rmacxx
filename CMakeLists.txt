cmake_minimum_required (VERSION 3.20)

# set the project name
project(rmacxx LANGUAGES CXX)

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------
# RMACXX_EMULATOR=ON builds the tests against a CPU emulation of CUDA/NVSHMEM/MPI
# (tests/emulator, one PE per thread). No GPU, CUDA toolkit, NVSHMEM or MPI is
# required; this is what CI uses. Default OFF: real CUDA + NVSHMEM build.
option(RMACXX_EMULATOR "Build tests against the CPU emulator (testing only)" OFF)
# Bootstrap NVSHMEM through MPI (launch with mpiexec). If OFF, NVSHMEM's own
# bootstrap is used (launch with nvshmrun / PMI).
option(RMACXX_USE_MPI_BOOTSTRAP "Bootstrap NVSHMEM with MPI" ON)
option(RMACXX_BUILD_TESTS "Build the tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_library(rmacxx INTERFACE)
target_include_directories(rmacxx INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include/)
target_compile_features(rmacxx INTERFACE cxx_std_20)

if (RMACXX_USE_MPI_BOOTSTRAP)
  target_compile_definitions(rmacxx INTERFACE RMACXX_USE_MPI_BOOTSTRAP=1)
else()
  target_compile_definitions(rmacxx INTERFACE RMACXX_USE_MPI_BOOTSTRAP=0)
endif()

if (NOT RMACXX_EMULATOR)
  # --------------------------------------------------------------------------
  # CUDA + NVSHMEM
  # --------------------------------------------------------------------------
  if (NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 80 90)
  endif()
  enable_language(CUDA)
  set(CMAKE_CUDA_STANDARD 20)
  set(CMAKE_CUDA_STANDARD_REQUIRED True)
  find_package(CUDAToolkit REQUIRED)

  # NVSHMEM ships a CMake package (lib/cmake/nvshmem); point NVSHMEM_HOME (or
  # NVSHMEM_DIR / CMAKE_PREFIX_PATH) at the installation if it is not found.
  set(NVSHMEM_HOME "$ENV{NVSHMEM_HOME}" CACHE PATH "NVSHMEM installation prefix")
  find_package(NVSHMEM CONFIG QUIET HINTS ${NVSHMEM_HOME} PATH_SUFFIXES lib/cmake/nvshmem)

  if (TARGET nvshmem::nvshmem_host AND TARGET nvshmem::nvshmem_device)
    target_link_libraries(rmacxx INTERFACE nvshmem::nvshmem_host nvshmem::nvshmem_device)
  elseif (TARGET nvshmem::nvshmem)
    target_link_libraries(rmacxx INTERFACE nvshmem::nvshmem)
  else()
    find_path(NVSHMEM_INCLUDE_DIR nvshmem.h HINTS ${NVSHMEM_HOME} PATH_SUFFIXES include REQUIRED)
    find_library(NVSHMEM_HOST_LIB nvshmem_host HINTS ${NVSHMEM_HOME} PATH_SUFFIXES lib lib64)
    find_library(NVSHMEM_DEVICE_LIB nvshmem_device HINTS ${NVSHMEM_HOME} PATH_SUFFIXES lib lib64)
    if (NVSHMEM_HOST_LIB AND NVSHMEM_DEVICE_LIB)
      target_link_libraries(rmacxx INTERFACE ${NVSHMEM_HOST_LIB} ${NVSHMEM_DEVICE_LIB})
    else()
      # older releases: single static library
      find_library(NVSHMEM_LIB nvshmem HINTS ${NVSHMEM_HOME} PATH_SUFFIXES lib lib64 REQUIRED)
      target_link_libraries(rmacxx INTERFACE ${NVSHMEM_LIB} CUDA::cuda_driver)
    endif()
    target_include_directories(rmacxx INTERFACE ${NVSHMEM_INCLUDE_DIR})
  endif()
  target_link_libraries(rmacxx INTERFACE CUDA::cudart)

  if (RMACXX_USE_MPI_BOOTSTRAP)
    find_package(MPI REQUIRED COMPONENTS CXX)
    target_link_libraries(rmacxx INTERFACE MPI::MPI_CXX)
  endif()
endif()

# ----------------------------------------------------------------------------
# Tests
# ----------------------------------------------------------------------------
if (NOT RMACXX_BUILD_TESTS)
  return()
endif()

enable_testing()
include(CTest)

set(TEST_DIR tests)

file(GLOB Level1 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*/*.cpp)
file(GLOB Level2 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_DIR}/*/*/*.cpp)
set(TestsToRun ${Level1} ${Level2})
list(FILTER TestsToRun EXCLUDE REGEX "^emulator/")

function(get_test_info path_var without_ext_var test_name_var num_processes_var)
  cmake_path(REMOVE_EXTENSION ${path_var} LAST_ONLY OUTPUT_VARIABLE without_ext) # Remove cpp
  cmake_path(GET without_ext EXTENSION num_from_ext) # Get potential number
  if(num_from_ext STREQUAL "")
    set(new_path ${without_ext})
    set(num_processes 2)
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

if (RMACXX_EMULATOR)
  find_package(Threads REQUIRED)
  add_library(rmacxx_emu_main OBJECT ${PROJECT_SOURCE_DIR}/tests/emulator/emu_main.cpp)
  target_include_directories(rmacxx_emu_main PRIVATE ${PROJECT_SOURCE_DIR}/tests/emulator/include)
endif()

if (NOT RMACXX_EMULATOR AND NOT RMACXX_USE_MPI_BOOTSTRAP)
  find_program(NVSHMRUN nvshmrun nvshmrun.hydra HINTS ${NVSHMEM_HOME}/bin REQUIRED)
endif()

# Add all the ADD_TEST for each test
foreach (test ${TestsToRun})
  get_test_info(test no_ext test_name test_processes)
  set(executable_name ${test_name}.${test_processes})
  set(src ${PROJECT_SOURCE_DIR}/${TEST_DIR}/${test})
  add_executable(${executable_name} ${src})
  target_compile_features(${executable_name} PRIVATE cxx_std_20)
  target_link_libraries(${executable_name} PRIVATE rmacxx)

  if (RMACXX_EMULATOR)
    # one PE per thread; the test's main becomes rmacxx_test_main
    target_include_directories(${executable_name} BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/tests/emulator/include)
    target_compile_definitions(${executable_name} PRIVATE main=rmacxx_test_main)
    if (test MATCHES "^futures/")
      # FuturesManager keeps its buffers in (here: thread_local) static storage
      target_compile_definitions(${executable_name} PRIVATE DEFAULT_EEXPR_SIZE=4096 DEFAULT_BEXPR_SIZE=16384)
    endif()
    target_sources(${executable_name} PRIVATE $<TARGET_OBJECTS:rmacxx_emu_main>)
    target_link_libraries(${executable_name} PRIVATE Threads::Threads)
    add_test (NAME ${executable_name} COMMAND ${PROJECT_BINARY_DIR}/${executable_name})
    set_tests_properties(${executable_name} PROPERTIES ENVIRONMENT "EMU_NP=${test_processes}" TIMEOUT 300)
  else()
    # tests include rmacxx.hpp, which contains device code: compile as CUDA
    # with relocatable device code (required by NVSHMEM)
    set_source_files_properties(${src} PROPERTIES LANGUAGE CUDA)
    set_target_properties(${executable_name} PROPERTIES
      CUDA_SEPARABLE_COMPILATION ON
      CUDA_RESOLVE_DEVICE_SYMBOLS ON
      LINKER_LANGUAGE CUDA)
    if (RMACXX_USE_MPI_BOOTSTRAP)
      add_test (NAME ${executable_name} COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} ${test_processes}
                ${MPIEXEC_PREFLAGS} ${PROJECT_BINARY_DIR}/${executable_name} ${MPIEXEC_POSTFLAGS})
    else()
      add_test (NAME ${executable_name} COMMAND ${NVSHMRUN} -np ${test_processes} ${PROJECT_BINARY_DIR}/${executable_name})
    endif()
  endif()
endforeach ()
