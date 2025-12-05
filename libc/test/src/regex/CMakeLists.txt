add_custom_target(libc_regex_unittests)

add_libc_unittest(
  regerror_test
  SUITE
    libc_regex_unittests
  SRCS
    regerror_test.cpp
  DEPENDS
    libc.include.regex
    libc.src.regex.regerror
)
