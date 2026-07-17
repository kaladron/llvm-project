add_custom_target(libc-rpc-tests)

add_libc_test(
  rpc_smoke_test
  SUITE
    libc-rpc-tests
  SRCS
    rpc_smoke_test.cpp
  DEPENDS
    libc.shared.rpc
)
