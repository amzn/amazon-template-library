##############################################################################
# This CMake file provides the following high-level targets:
#
# <nothing>:
#   If no target is specified, the AmazonTemplateLibrary library is built
#   and installed.
#
# release:
#   Build and install the library, and run the unit tests.
#
# build-tests:
#   Build the unit tests, but don't run them.
#
# test:
#   Build and then run the unit tests.
#
# test-valgrind:
#   Build and then run the unit tests under Valgrind. Only available when
#   Valgrind is available.
#
# install:
#   Build and install the library.
##############################################################################
cmake_minimum_required(VERSION 3.8.0)

project(AmazonTemplateLibrary VERSION 1.0.0
                              LANGUAGES CXX)

find_package(Boost 1.63 REQUIRED) # Boost.Intrusive < 1.63 has a bug that breaks us

# Setup the `atl` library target and export it
add_library(atl INTERFACE)
target_include_directories(atl INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_compile_features(atl INTERFACE cxx_std_14)
target_link_libraries(atl INTERFACE Boost::boost)
install(TARGETS atl
        EXPORT ${PROJECT_NAME}Config
        INCLUDES DESTINATION include)
install(EXPORT ${PROJECT_NAME}Config
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME})
install(DIRECTORY include/amz
        DESTINATION include
        FILES_MATCHING PATTERN "*.hpp")

# Setup unit tests
enable_testing()
add_custom_target(build-tests COMMENT "Build all the unit tests.")
add_test(NAME build-tests
         COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target build-tests)
add_subdirectory(test)

# Setup a default target that runs when no target is specified.
add_custom_target(default ALL
  COMMENT "Build and install the library."
  COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_install.cmake"
  USES_TERMINAL)

# Setup the 'release' target (which is run on Package Builder).
add_custom_target(release
  COMMENT "Build and install the library, and run the unit tests."
  COMMAND "${CMAKE_COMMAND}" -E chdir "${CMAKE_BINARY_DIR}" "${CMAKE_CTEST_COMMAND}" --output-on-failure
  COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target install
  DEPENDS build-tests
  USES_TERMINAL)
