add_custom_target(libc_langinfo_unittests)

add_libc_unittest(
  nl_langinfo_test
  SUITE
    libc_langinfo_unittests
  SRCS
    nl_langinfo_test.cpp
  DEPENDS
    libc.hdr.langinfo_macros
    libc.src.langinfo.nl_langinfo
    libc.src.langinfo.nl_langinfo_l
)
