#===============================================================================
# Lit Test Infrastructure Setup
#===============================================================================
if(LIBC_ENABLE_LIT_TESTS)
  # Configure lit site config file
  configure_libc_lit_site_cfg()
  
  # Create the umbrella lit test target
  add_custom_target(check-libc-lit
    COMMENT "Running all libc lit tests"
  )
  
  message(STATUS "libc: Lit test infrastructure enabled")
endif()

#===============================================================================
# Legacy Test Infrastructure Setup  
#===============================================================================
if(NOT LIBC_ENABLE_LEGACY_TESTS)
  # Skip legacy test setup - return early
  # But still process lit tests if enabled above
  return()
endif()

# --- Existing test setup code follows ---

# Legacy test targets
add_custom_target(libc-unit-tests)
add_custom_target(libc-hermetic-tests)

add_custom_target(exhaustive-check-libc)
add_custom_target(libc-long-running-tests)

add_subdirectory(UnitTest)

if(LIBC_TARGET_OS_IS_GPU)
  if(NOT TARGET libc.utils.gpu.loader)
    message(WARNING "Cannot build libc GPU tests, missing loader.")
    return()
  elseif(LIBC_GPU_TESTS_DISABLED)
    message(WARNING "Cannot build libc GPU tests, missing target architecture.")
    return()
  endif()
endif()

add_subdirectory(src)
add_subdirectory(utils)
add_subdirectory(shared)

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
# Unified check-libc Target
#===============================================================================
# Create the main check-libc target that runs the appropriate tests
if(NOT TARGET check-libc)
  add_custom_target(check-libc)
endif()

if(LIBC_ENABLE_LIT_TESTS AND TARGET check-libc-lit)
  add_dependencies(check-libc check-libc-lit)
endif()

if(LIBC_ENABLE_LEGACY_TESTS AND TARGET libc-unit-tests)
  add_dependencies(check-libc libc-unit-tests)
endif()

if(LIBC_ENABLE_LEGACY_TESTS AND TARGET libc-hermetic-tests)
  add_dependencies(check-libc libc-hermetic-tests)
endif()


