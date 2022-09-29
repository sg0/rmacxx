enable_testing()
include(CTest)

file(GLOB TestsToRun RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp)

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
  set(${test_name_var} ${new_path} PARENT_SCOPE)
  set(${without_ext_var} ${without_ext} PARENT_SCOPE)
  set(${num_processes_var} ${num_processes} PARENT_SCOPE)
endfunction()

# Add all the ADD_TEST for each test
foreach (test ${TestsToRun})
  get_test_info(test no_ext test_name test_processes)
  message(WARNING '${no_ext}''${test_name}' '${test_processes}')
  get_filename_component(EXECUTABLE_NAME ${test} NAME_WE)
  get_filename_component(EXECUTABLE_DIR ${test} NAME_WE)
  set(executable ${EXECUTABLE_DIR}_${EXECUTABLE_NAME})
  add_executable(${executable} ${test})
  target_compile_features(${executable} PRIVATE cxx_std_20)
  target_include_directories(${executable} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  add_test (NAME ${test_name} COMMAND mpiexec ${executable} --np ${test_processes})
endforeach ()




# enable_testing()
# include(CTest)

# file(GLOB SOURCE_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp)

# create_test_sourcelist (Tests RMACXX_Tests.cpp ${SOURCE_FILES})

# # add the executable
# add_executable (RMACXX_Tests ${Tests})
# target_compile_features(RMACXX_Tests PRIVATE cxx_std_20)
# target_include_directories(RMACXX_Tests PRIVATE ${PROJECT_SOURCE_DIR}/include)


# # remove the test driver source file
# set (TestsToRun ${Tests})
# remove (TestsToRun RMACXX_Tests.cpp)

# function(get_test_info path_var without_ext_var test_name_var num_processes_var)
#   cmake_path(REMOVE_EXTENSION ${path_var} LAST_ONLY OUTPUT_VARIABLE without_ext) # Remove cpp
#   cmake_path(GET without_ext EXTENSION num_from_ext) # Get potential number
#   if(num_from_ext STREQUAL "")
#     set(new_path ${without_ext})
#     set(num_processes 2)
#   else()              # optional block
#     string(SUBSTRING ${num_from_ext} 1 -1 num_from_ext_no_period)
#     cmake_path(REMOVE_EXTENSION ${path_var} OUTPUT_VARIABLE new_path)
#     cmake_path(REMOVE_EXTENSION num_from_ext_no_period OUTPUT_VARIABLE num_processes)
#   endif()
#   set(${test_name_var} ${new_path} PARENT_SCOPE)
#   set(${without_ext_var} ${without_ext} PARENT_SCOPE)
#   set(${num_processes_var} ${num_processes} PARENT_SCOPE)
# endfunction()

# # Add all the ADD_TEST for each test
# foreach (test ${TestsToRun})
#   get_test_info(test no_ext test_name test_processes)
#   # message(WARNING '${no_ext}''${test_name}' '${test_processes}')
#   get_filename_component(EXECUTABLE_NAME ${test} NAME_WE)
#   get_filename_component(EXECUTABLE_DIR ${test} NAME_WE)
#   add_test (NAME ${test_name} COMMAND mpiexec RMACXX_Tests ${test} --np ${test_processes})
# endforeach ()
