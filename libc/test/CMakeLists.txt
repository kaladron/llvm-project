#===============================================================================
# Lit Test Infrastructure Setup
#===============================================================================
if(LIBC_ENABLE_LIT_TESTS)
  # Include lit test rules and functions
  include(LLVMLibCLitTestRules)
  
  # Configure lit site config file
  configure_libc_lit_site_cfg()
  
  # Create the umbrella lit test target
  add_custom_target(check-libc-lit
    COMMENT "Running all libc lit tests"
  )
  
  message(STATUS "libc: Lit test infrastructure enabled")
endif()

#===============================================================================
# Create unified check-libc target early
#===============================================================================
# This must be created before subdirectories that may add dependencies to it
if(NOT TARGET check-libc)
  add_custom_target(check-libc)
endif()

if(LIBC_ENABLE_LIT_TESTS AND TARGET check-libc-lit)
  add_dependencies(check-libc check-libc-lit)
endif()

#===============================================================================
# Process test subdirectories (needed for both lit and legacy)
#===============================================================================
# UnitTest framework is needed for both systems (provides LibcTest.unit)
add_subdirectory(UnitTest)
add_subdirectory(src)
add_subdirectory(utils)
add_subdirectory(shared)

#===============================================================================
# Legacy Test Infrastructure Setup  
#===============================================================================
if(NOT LIBC_ENABLE_LEGACY_TESTS)
  # Skip legacy test setup - all remaining setup is legacy-specific
  return()
endif()

# --- Existing test setup code follows ---

# Legacy test targets
add_custom_target(libc-unit-tests)
add_custom_target(libc-hermetic-tests)

add_custom_target(exhaustive-check-libc)
add_custom_target(libc-long-running-tests)

if(LIBC_TARGET_OS_IS_GPU)
  if(NOT TARGET libc.utils.gpu.loader)
    message(WARNING "Cannot build libc GPU tests, missing loader.")
    return()
  elseif(LIBC_GPU_TESTS_DISABLED)
    message(WARNING "Cannot build libc GPU tests, missing target architecture.")
    return()
  endif()
endif()

if(NOT LLVM_LIBC_FULL_BUILD)
  return()
endif()

add_subdirectory(include)

if(NOT ${LIBC_TARGET_OS} STREQUAL "linux" AND
   NOT ${LIBC_TARGET_OS} STREQUAL "gpu")
  # Integration tests are currently only available for linux and the GPU.
  return()
endif()

add_subdirectory(IntegrationTest)
add_subdirectory(integration)

#===============================================================================
# Add legacy test targets to check-libc
#===============================================================================
if(LIBC_ENABLE_LEGACY_TESTS AND TARGET libc-unit-tests)
  add_dependencies(check-libc libc-unit-tests)
endif()

if(LIBC_ENABLE_LEGACY_TESTS AND TARGET libc-hermetic-tests)
  add_dependencies(check-libc libc-hermetic-tests)
endif()



