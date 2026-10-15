add_custom_target(libc_ucontext_unittests)

if(TARGET libc.src.ucontext.getcontext)
  add_libc_test(
    ucontext_test
    SUITE
      libc_ucontext_unittests
    SRCS
      ucontext_test.cpp
    DEPENDS
      libc.hdr.signal_macros
      libc.hdr.stdint_proxy
      libc.include.llvm-libc-types.ucontext_t
      libc.src.ucontext.getcontext
      libc.src.ucontext.makecontext
      libc.src.ucontext.setcontext
      libc.src.signal.sigemptyset
      libc.src.signal.sigaddset
      libc.src.signal.sigprocmask
  )
endif()
