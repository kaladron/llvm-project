#===============================================================================
# libc ctype tests
#
# This file supports both legacy and lit test infrastructure during migration.
# The dual approach allows testing the new lit system without breaking existing
# workflows.
#===============================================================================

add_custom_target(libc-ctype-tests)

#===============================================================================
# Helper macro to define a test for both systems
#===============================================================================
macro(add_ctype_test test_name test_src test_dep)
  # Legacy system: creates individual test targets
  if(LIBC_ENABLE_LEGACY_TESTS)
    add_libc_test(
      ${test_name}
      SUITE
        libc-ctype-tests
      SRCS
        ${test_src}
      DEPENDS
        ${test_dep}
    )
  endif()
  
  # Lit system: collect sources for combined executable
  if(LIBC_ENABLE_LIT_TESTS)
    list(APPEND CTYPE_LIT_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/${test_src})
    list(APPEND CTYPE_LIT_DEPS ${test_dep})
  endif()
endmacro()

#===============================================================================
# Initialize lit source collection
#===============================================================================
if(LIBC_ENABLE_LIT_TESTS)
  set(CTYPE_LIT_SRCS "")
  set(CTYPE_LIT_DEPS "")
endif()

#===============================================================================
# Test definitions (shared format for both systems)
#===============================================================================
add_ctype_test(isalnum_test isalnum_test.cpp libc.src.ctype.isalnum)
add_ctype_test(isalpha_test isalpha_test.cpp libc.src.ctype.isalpha)
add_ctype_test(isascii_test isascii_test.cpp libc.src.ctype.isascii)
add_ctype_test(isblank_test isblank_test.cpp libc.src.ctype.isblank)
add_ctype_test(iscntrl_test iscntrl_test.cpp libc.src.ctype.iscntrl)
add_ctype_test(isdigit_test isdigit_test.cpp libc.src.ctype.isdigit)
add_ctype_test(isgraph_test isgraph_test.cpp libc.src.ctype.isgraph)
add_ctype_test(islower_test islower_test.cpp libc.src.ctype.islower)
add_ctype_test(isprint_test isprint_test.cpp libc.src.ctype.isprint)
add_ctype_test(ispunct_test ispunct_test.cpp libc.src.ctype.ispunct)
add_ctype_test(isspace_test isspace_test.cpp libc.src.ctype.isspace)
add_ctype_test(isupper_test isupper_test.cpp libc.src.ctype.isupper)
add_ctype_test(isxdigit_test isxdigit_test.cpp libc.src.ctype.isxdigit)
add_ctype_test(toascii_test toascii_test.cpp libc.src.ctype.toascii)
add_ctype_test(tolower_test tolower_test.cpp libc.src.ctype.tolower)
add_ctype_test(toupper_test toupper_test.cpp libc.src.ctype.toupper)

#===============================================================================
# Lit Test Executable (when LIBC_ENABLE_LIT_TESTS=ON)
#===============================================================================
if(LIBC_ENABLE_LIT_TESTS)
  # Remove duplicate dependencies
  list(REMOVE_DUPLICATES CTYPE_LIT_DEPS)
  
  # Create a single test executable containing all ctype tests
  # All tests use static registration - they'll all run from one main()
  add_libc_lit_test_executable(LibcCtypeTests
    SRCS ${CTYPE_LIT_SRCS}
    DEPENDS ${CTYPE_LIT_DEPS}
  )
  
  # Add a convenience target to build and run just ctype tests
  add_custom_target(check-libc-ctype-lit
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/LibcCtypeTests
    DEPENDS LibcCtypeTests
    COMMENT "Running libc ctype tests (lit)"
    USES_TERMINAL
  )
  
  # Add to umbrella lit target
  if(TARGET check-libc-lit)
    add_dependencies(check-libc-lit check-libc-ctype-lit)
  endif()
endif()
