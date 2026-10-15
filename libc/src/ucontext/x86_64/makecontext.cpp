//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of makecontext for x86_64.
///
//===----------------------------------------------------------------------===//

#include "src/ucontext/makecontext.h"

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "include/llvm-libc-types/ucontext_t.h"
#include "src/__support/OSUtil/exit.h"
#include "src/__support/common.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/ucontext/setcontext.h"
#include <stdarg.h>

namespace LIBC_NAMESPACE_DECL {

// Trampoline invoked when the function registered with makecontext returns.
// When func returns via `retq`, RSP is 16-byte aligned (rather than the usual
// 16n - 8 on call entry), so `force_align_arg_pointer` realigns RSP before any
// outgoing calls. The incoming RBP is callee-saved by func and still points to
// the synthetic frame record `[0, uc_link]` placed at the top of uc_stack by
// makecontext, which `__builtin_frame_address(1)` reads directly.
[[noreturn]] __attribute__((force_align_arg_pointer)) static void
context_trampoline() noexcept {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wframe-address"
  auto **frame = reinterpret_cast<ucontext_t **>(__builtin_frame_address(1));
#pragma clang diagnostic pop
  ucontext_t *uc_link = frame[1];
  if (uc_link != nullptr)
    LIBC_NAMESPACE::setcontext(uc_link);
  LIBC_NAMESPACE::internal::exit(0);
}

LLVM_LIBC_FUNCTION(void, makecontext,
                   (ucontext_t * ucp, void (*func)(void), int argc,
                    ...)) noexcept {
  LIBC_CRASH_ON_NULLPTR(ucp);
  LIBC_CRASH_ON_NULLPTR(func);
  LIBC_CRASH_ON_NULLPTR(ucp->uc_stack.ss_sp);

  // Reserve a 2-word frame record `[0, uc_link]` at the top of the stack for
  // `context_trampoline` (and to terminate frame-pointer stack walks), followed
  // by any stack-passed arguments (beyond the 6 register arguments). Align the
  // argument area down to 16 bytes, then push the return address so that
  // RSP % 16 == 8 on entry to func as required by the x86_64 System V ABI.
  uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(ucp->uc_stack.ss_sp) + ucp->uc_stack.ss_size;
  const size_t STACK_ARGS = argc > 6 ? static_cast<size_t>(argc - 6) : 0;
  stack_top -= (STACK_ARGS + 2) * sizeof(uintptr_t);
  stack_top &= ~static_cast<uintptr_t>(15);

  uintptr_t *sp = reinterpret_cast<uintptr_t *>(stack_top) - 1;
  sp[0] = reinterpret_cast<uintptr_t>(context_trampoline);

  uintptr_t *frame = &sp[1 + STACK_ARGS];
  frame[0] = 0;
  frame[1] = reinterpret_cast<uintptr_t>(ucp->uc_link);

  ucp->uc_mcontext.gregs[REG_RIP] = reinterpret_cast<greg_t>(func);
  ucp->uc_mcontext.gregs[REG_RSP] = reinterpret_cast<greg_t>(sp);
  ucp->uc_mcontext.gregs[REG_RBP] = reinterpret_cast<greg_t>(frame);

  va_list ap;
  va_start(ap, argc);
  for (int i = 0; i < argc; ++i) {
    greg_t arg = static_cast<greg_t>(va_arg(ap, int));
    switch (i) {
    case 0:
      ucp->uc_mcontext.gregs[REG_RDI] = arg;
      break;
    case 1:
      ucp->uc_mcontext.gregs[REG_RSI] = arg;
      break;
    case 2:
      ucp->uc_mcontext.gregs[REG_RDX] = arg;
      break;
    case 3:
      ucp->uc_mcontext.gregs[REG_RCX] = arg;
      break;
    case 4:
      ucp->uc_mcontext.gregs[REG_R8] = arg;
      break;
    case 5:
      ucp->uc_mcontext.gregs[REG_R9] = arg;
      break;
    default:
      sp[i - 5] = static_cast<uintptr_t>(arg);
      break;
    }
  }
  va_end(ap);
}

} // namespace LIBC_NAMESPACE_DECL
