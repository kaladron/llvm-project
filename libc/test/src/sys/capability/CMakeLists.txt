add_custom_target(libc-sys-capability-tests)

add_libc_test(
  capget_test
  SUITE
    libc-sys-capability-tests
  SRCS
    capget_test.cpp
  DEPENDS
    libc.hdr.errno_macros
    libc.src.sys.capability.capget
    libc.test.UnitTest.ErrnoCheckingTest
    libc.test.UnitTest.ErrnoSetterMatcher
)

add_libc_test(
  capset_test
  SUITE
    libc-sys-capability-tests
  SRCS
    capset_test.cpp
  DEPENDS
    libc.hdr.errno_macros
    libc.src.sys.capability.capget
    libc.src.sys.capability.capset
    libc.test.UnitTest.ErrnoCheckingTest
    libc.test.UnitTest.ErrnoSetterMatcher
)
