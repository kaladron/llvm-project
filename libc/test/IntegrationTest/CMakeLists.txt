set(arch_specific_deps)
if(LIBC_TARGET_ARCHITECTURE_IS_AARCH64)
  set(arch_specific_deps libc.src.sys.auxv.getauxval)
endif()

if(LIBC_ENABLE_COVERAGE)
  set(PROFILE_DIR "${LIBC_SOURCE_DIR}/../compiler-rt/lib/profile")
  add_object_library(
    profile_baremetal
    SRCS
      ${PROFILE_DIR}/InstrProfiling.c
      ${PROFILE_DIR}/InstrProfilingInternal.c
      ${PROFILE_DIR}/InstrProfilingBuffer.c
      ${PROFILE_DIR}/InstrProfilingMerge.c
      ${PROFILE_DIR}/InstrProfilingNameVar.c
      ${PROFILE_DIR}/InstrProfilingVersionVar.c
      ${PROFILE_DIR}/InstrProfilingWriter.c
      ${PROFILE_DIR}/InstrProfilingPlatformLinux.c
    COMPILE_OPTIONS
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
