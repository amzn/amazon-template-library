find_program(MEMORYCHECK_COMMAND NAMES valgrind-wrapper valgrind)
if (MEMORYCHECK_COMMAND)
  message(STATUS "Found Valgrind: ${MEMORYCHECK_COMMAND}")
  set(MEMORYCHECK_COMMAND_OPTIONS "--leak-check=full --gen-suppressions=all --error-exitcode=1")
  set(MEMORYCHECK_SUPPRESSIONS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/valgrind.supp")
  add_custom_target(test-valgrind
    COMMENT "Build and then run all the unit tests under a memory checker."
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -D ExperimentalMemCheck
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    DEPENDS build-tests
    USES_TERMINAL)
  include(CTest)
else()
  message(STATUS "Valgrind not found; the test-valgrind target won't be available")
endif()

# add_test(...)
#
# Equivalent to a vanilla `add_test`, but adds a dependency on the
# `build-tests` test. This way, running the `test` target will build
# the unit tests before trying to run them, which would otherwise fail.
function(add_test)
  cmake_parse_arguments(ARG "" "NAME" "" ${ARGN})
  _add_test(${ARGN})
  set_property(TEST ${ARG_NAME} APPEND
               PROPERTY DEPENDS build-tests)
endfunction()

# get_target_name(<target> <file>)
#
# Retrieves the name of the unit test target associated to a single source file.
function(get_target_name target file)
  string(REPLACE ".cpp" "" _target "${file}")
  string(REPLACE "/" "." _target "${_target}")
  set(${target} "test.${_target}" PARENT_SCOPE)
endfunction()

find_package(Catch2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS filesystem)

# add_test_executable(<target> <file>)
#
# Creates a test executable for the given source file.
function(add_test_executable target file)
  add_executable(${target} EXCLUDE_FROM_ALL "${file}")
  target_link_libraries(${target} PRIVATE atl Catch2::Catch Boost::boost Boost::filesystem)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS NO)
  target_compile_options(${target} PRIVATE -Wall)
  add_dependencies(build-tests ${target})
endfunction()

# Add all the unit tests.
file(GLOB_RECURSE UNIT_TESTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp")
foreach(testfile IN LISTS UNIT_TESTS)
  get_target_name(target "${testfile}")
  if (NOT TARGET ${target})
    add_test_executable(${target} "${testfile}")
    add_test(NAME ${target} COMMAND ${target}
             WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
  endif()
endforeach()

# For the special multi-process unit test, make sure tests are run in the
# correct order.
set_tests_properties(test.deferred_reclamation_allocator.interprocess.0_setup PROPERTIES FIXTURES_SETUP deferred_alloc_ipc)
set_tests_properties(test.deferred_reclamation_allocator.interprocess.2_cleanup PROPERTIES FIXTURES_CLEANUP deferred_alloc_ipc)
set_tests_properties(test.deferred_reclamation_allocator.interprocess.1_allocate PROPERTIES FIXTURES_REQUIRED deferred_alloc_ipc)
