add_libc_testsuite(libc-iconv-tests)

add_libc_test(
  iconv_open_close_test
  SUITE
    libc-iconv-tests
  SRCS
    iconv_open_close_test.cpp
  DEPENDS
    libc.hdr.errno_macros
    libc.hdr.types.iconv_t
    libc.src.__support.CPP.scope
    libc.src.iconv.iconv_close
    libc.src.iconv.iconv_open
    libc.test.UnitTest.ErrnoCheckingTest
    libc.test.UnitTest.ErrnoSetterMatcher
)
