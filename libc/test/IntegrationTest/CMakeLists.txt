set(arch_specific_deps)
if(LIBC_TARGET_ARCHITECTURE_IS_AARCH64)
  set(arch_specific_deps libc.src.sys.auxv.getauxval)
endif()

if(LIBC_ENABLE_COVERAGE)
  set(PROFILE_DIR "${LIBC_SOURCE_DIR}/../compiler-rt/lib/profile")
  # Use a raw add_library instead of add_object_library because the profile
  # runtime is compiler-rt code, not libc code. add_object_library adds
  # -isystem ${LIBC_INCLUDE_DIR} which causes typedef conflicts (e.g.,
  # locale_t) between LLVM libc's generated headers and the system headers
  # that compiler-rt expects to use.
  set(profile_baremetal_fq_name libc.test.IntegrationTest.profile_baremetal)
  add_library(
    ${profile_baremetal_fq_name}
    EXCLUDE_FROM_ALL
    OBJECT
    ${PROFILE_DIR}/InstrProfiling.c
    ${PROFILE_DIR}/InstrProfilingInternal.c
    ${PROFILE_DIR}/InstrProfilingBuffer.c
    ${PROFILE_DIR}/InstrProfilingMerge.c
    ${PROFILE_DIR}/InstrProfilingNameVar.c
    ${PROFILE_DIR}/InstrProfilingVersionVar.c
    ${PROFILE_DIR}/InstrProfilingWriter.c
    ${PROFILE_DIR}/InstrProfilingPlatformLinux.c
  )
  target_compile_options(${profile_baremetal_fq_name} PRIVATE
    -DCOMPILER_RT_PROFILE_BAREMETAL=1
    -I${PROFILE_DIR}
    -I${LIBC_SOURCE_DIR}/../compiler-rt/lib
    -fno-profile-instr-generate
    -fno-coverage-mapping
    -Wno-strict-prototypes
    -Wno-shorten-64-to-32
    -Wno-unused-parameter
    -Wno-implicit-int-conversion
  )
  set_target_properties(
    ${profile_baremetal_fq_name}
    PROPERTIES
      TARGET_TYPE "OBJECT_LIBRARY"
      OBJECT_FILES "$<TARGET_OBJECTS:${profile_baremetal_fq_name}>"
  )

  # Lightweight coverage data writer for hermetic tests. Uses the profile
  # runtime buffer API (__llvm_profile_write_buffer) and writes the raw
  # profile data to disk using raw Linux syscalls via inline assembly.
  # This avoids any dependency on stdio/libc functions.
  set(coverage_dump_fq_name libc.test.IntegrationTest.coverage_dump)
  add_library(
    ${coverage_dump_fq_name}
    EXCLUDE_FROM_ALL
    OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/coverage_dump.cpp
  )
  target_compile_options(${coverage_dump_fq_name} PRIVATE
    -fno-profile-instr-generate
    -fno-coverage-mapping
    -fno-stack-protector
  )
  set_target_properties(
    ${coverage_dump_fq_name}
    PROPERTIES
      TARGET_TYPE "OBJECT_LIBRARY"
      OBJECT_FILES "$<TARGET_OBJECTS:${coverage_dump_fq_name}>"
  )
endif()

set(test_deps
  libc.hdr.stdint_proxy
  libc.src.__support.OSUtil.osutil
  libc.src.__support.CPP.atomic
  libc.src.__support.macros.properties.architectures
  ${arch_specific_deps}
)

if(LIBC_ENABLE_COVERAGE)
  list(APPEND test_deps libc.test.IntegrationTest.profile_baremetal)
endif()

add_object_library(
  test
  SRCS
    test.cpp
  COMPILE_OPTIONS
    ${TEST_COMPILE_FLAGS}
  HDRS
    test.h
  DEPENDS
    ${test_deps}
)

if(LIBC_ENABLE_COVERAGE)
  target_compile_definitions(libc.test.IntegrationTest.test PRIVATE LIBC_ENABLE_COVERAGE)
endif()
