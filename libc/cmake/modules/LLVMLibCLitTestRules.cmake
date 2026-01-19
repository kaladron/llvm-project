#===============================================================================
# Lit Test Infrastructure for LLVM libc
#
# This module provides functions to set up lit-based testing for libc.
# It is only active when LIBC_ENABLE_LIT_TESTS=ON.
#
# Key insight: The libc test framework uses static registration. When you
# define a TEST() macro, it creates a static global object that registers
# itself with the test runner at program startup. This means we can link
# multiple test files together and they will all be discovered and run.
#
# The main() function is provided by LibcTestMain.cpp (part of LibcTest.unit).
#===============================================================================

# Guard against double inclusion
if(LIBC_LIT_TEST_RULES_INCLUDED)
  return()
endif()
set(LIBC_LIT_TEST_RULES_INCLUDED TRUE)

# Include LLVM's lit infrastructure
include(AddLLVM)

#-------------------------------------------------------------------------------
# configure_libc_lit_site_cfg()
#
# Configures the lit.site.cfg.py file from its template.
# This should be called once from libc/test/CMakeLists.txt.
#-------------------------------------------------------------------------------
function(configure_libc_lit_site_cfg)
  # Set variables for the template
  set(LIBC_SOURCE_DIR "${LIBC_SOURCE_DIR}")
  set(LIBC_BINARY_DIR "${LIBC_BUILD_DIR}")
  
  # Handle LLVM_LIBC_FULL_BUILD as Python boolean
  if(LLVM_LIBC_FULL_BUILD)
    set(LLVM_LIBC_FULL_BUILD_LIT "True")
  else()
    set(LLVM_LIBC_FULL_BUILD_LIT "False")
  endif()
  
  # Set the test runner path (will be set per-suite later)
  set(LIBC_TEST_RUNNER "")
  
  # Configure the site config file
  configure_lit_site_cfg(
    ${LIBC_SOURCE_DIR}/test/lit.site.cfg.py.in
    ${LIBC_BUILD_DIR}/test/lit.site.cfg.py
    MAIN_CONFIG
    ${LIBC_SOURCE_DIR}/test/lit.cfg.py
    PATHS
    "LLVM_SOURCE_DIR"
    "LLVM_BINARY_DIR"
    "LLVM_TOOLS_DIR"
    "LLVM_LIBS_DIR"
    "LIBC_SOURCE_DIR"
    "LIBC_BINARY_DIR"
    "LIBC_TEST_RUNNER"
  )
endfunction()

#-------------------------------------------------------------------------------
# add_libc_lit_testsuite()
#
# Creates a lit test suite target.
#
# Usage:
#   add_libc_lit_testsuite(check-libc-ctype
#     SUITE_NAME ctype
#     TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ctype
#     DEPENDS libc.src.ctype
#   )
#-------------------------------------------------------------------------------
function(add_libc_lit_testsuite target_name)
  cmake_parse_arguments(
    "LIT_SUITE"
    ""                    # No optional arguments
    "SUITE_NAME;TEST_DIR" # Single value arguments
    "DEPENDS"             # Multi value arguments
    ${ARGN}
  )
  
  if(NOT LIT_SUITE_TEST_DIR)
    message(FATAL_ERROR "add_libc_lit_testsuite requires TEST_DIR")
  endif()
  
  # Create the lit test target using LLVM's infrastructure
  add_lit_testsuite(${target_name}
    "Running ${LIT_SUITE_SUITE_NAME} tests"
    ${LIT_SUITE_TEST_DIR}
    DEPENDS ${LIT_SUITE_DEPENDS}
  )
  
  # Add to the umbrella check-libc-lit target if it exists
  if(TARGET check-libc-lit)
    add_dependencies(check-libc-lit ${target_name})
  endif()
endfunction()

#-------------------------------------------------------------------------------
# add_libc_lit_test_executable()
#
# Creates a test executable that can be run by lit.
# This compiles multiple test source files into a single executable.
# The main() function comes from LibcTest.unit (LibcTestMain.cpp).
#
# How it works:
# - Each test file uses TEST() macro which creates a static global
# - At static init time, each test registers itself with the test runner
# - When main() runs, it iterates through all registered tests
# - Multiple test files can be linked together - all tests will run
#
# Usage:
#   add_libc_lit_test_executable(LibcCtypeTests
#     SRCS
#       isalnum_test.cpp
#       isalpha_test.cpp
#     DEPENDS
#       libc.src.ctype.isalnum
#       libc.src.ctype.isalpha
#   )
#-------------------------------------------------------------------------------
function(add_libc_lit_test_executable target_name)
  cmake_parse_arguments(
    "LIT_EXE"
    ""           # No optional arguments
    ""           # No single value arguments
    "SRCS;HDRS;DEPENDS;COMPILE_OPTIONS;LINK_LIBRARIES"  # Multi value arguments
    ${ARGN}
  )
  
  if(NOT LIT_EXE_SRCS)
    message(FATAL_ERROR "add_libc_lit_test_executable requires SRCS")
  endif()
  
  # Create the test executable
  # Note: main() is provided by LibcTest.unit which includes LibcTestMain.cpp
  add_executable(${target_name} EXCLUDE_FROM_ALL ${LIT_EXE_SRCS} ${LIT_EXE_HDRS})
  
  # Get compile options from the existing test infrastructure
  _get_common_test_compile_options(compile_options "" "")
  target_compile_options(${target_name} PRIVATE ${compile_options} ${LIT_EXE_COMPILE_OPTIONS})
  
  # Link against test framework (provides main() and TEST() infrastructure)
  # LibcTest.unit includes: LibcTest.cpp, LibcTestMain.cpp, TestLogger.cpp
  target_link_libraries(${target_name} PRIVATE
    LibcTest.unit
    ${LIT_EXE_LINK_LIBRARIES}
  )
  
  # Add include directories
  target_include_directories(${target_name} PRIVATE ${LIBC_SOURCE_DIR})
  
  # Handle libc dependencies (the actual libc functions being tested)
  if(LIT_EXE_DEPENDS)
    foreach(dep IN LISTS LIT_EXE_DEPENDS)
      if(TARGET ${dep})
        add_dependencies(${target_name} ${dep})
        get_target_property(dep_type ${dep} TYPE)
        # Link static/object libraries
        if(dep_type STREQUAL "STATIC_LIBRARY" OR dep_type STREQUAL "OBJECT_LIBRARY")
          target_link_libraries(${target_name} PRIVATE ${dep})
        endif()
      else()
        # Try as an object library with different naming
        string(REPLACE "." "_" dep_target ${dep})
        if(TARGET ${dep_target})
          target_link_libraries(${target_name} PRIVATE ${dep_target})
        endif()
      endif()
    endforeach()
  endif()
  
  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    FOLDER "libc/Tests/Lit"
  )
endfunction()
