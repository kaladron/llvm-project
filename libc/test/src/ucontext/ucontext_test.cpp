//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for ucontext routines.
///
//===----------------------------------------------------------------------===//

#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "include/llvm-libc-types/ucontext_t.h"
#include "src/signal/sigaddset.h"
#include "src/signal/sigemptyset.h"
#include "src/signal/sigprocmask.h"
#include "src/ucontext/getcontext.h"
#include "src/ucontext/makecontext.h"
#include "src/ucontext/setcontext.h"
#include "test/UnitTest/Test.h"

namespace {

constexpr uintptr_t STACK_ALIGNMENT = 16;

bool is_signal_set(const sigset_t *set, int signum) {
  // TODO: Replace this with sigismember once it is implemented.
  // sigset_t is an array of unsigned long, signum is 1-indexed.
  int word = (signum - 1) / (sizeof(unsigned long) * 8);
  int bit = (signum - 1) % (sizeof(unsigned long) * 8);
  return (set->__signals[word] & (1UL << bit)) != 0;
}

int g_invoked = 0;
uintptr_t g_frame_addr = 0;
int g_args[8] = {};

void no_arg_target() {
  g_invoked = 1;
  g_frame_addr = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
}

void six_arg_target(int a, int b, int c, int d, int e, int f) {
  g_invoked = 1;
  int vals[6] = {a, b, c, d, e, f};
  for (int i = 0; i < 6; ++i)
    g_args[i] = vals[i];
  g_frame_addr = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
}

void seven_arg_target(int a, int b, int c, int d, int e, int f, int g) {
  six_arg_target(a, b, c, d, e, f);
  g_args[6] = g;
  g_frame_addr = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
}

void eight_arg_target(int a, int b, int c, int d, int e, int f, int g, int h) {
  seven_arg_target(a, b, c, d, e, f, g);
  g_args[7] = h;
  g_frame_addr = reinterpret_cast<uintptr_t>(__builtin_frame_address(0));
}

} // namespace

TEST(LlvmLibcUcontextTest, BasicStubTest) {
  static volatile int jumped = 0;
  ucontext_t ctx;
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&ctx), 0);
  if (!jumped) {
    jumped = 1;
    LIBC_NAMESPACE::setcontext(&ctx);
    ASSERT_EQ(jumped, 0);
  }
}

TEST(LlvmLibcUcontextTest, SignalMaskTest) {
  sigset_t set, old_set;
  LIBC_NAMESPACE::sigemptyset(&set);
  LIBC_NAMESPACE::sigaddset(&set, SIGUSR1);

  // Set mask to [SIGUSR1] using sigprocmask
  ASSERT_EQ(LIBC_NAMESPACE::sigprocmask(SIG_SETMASK, &set, &old_set), 0);

  ucontext_t ctx;
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&ctx), 0);

  // Verify that getcontext captured the mask
  ASSERT_TRUE(is_signal_set(&ctx.uc_sigmask, SIGUSR1));
  ASSERT_FALSE(is_signal_set(&ctx.uc_sigmask, SIGUSR2));

  sigset_t new_set;
  static volatile int mask_jumped = 0;
  if (mask_jumped == 0) {
    mask_jumped = 1;
    LIBC_NAMESPACE::sigemptyset(&new_set);
    LIBC_NAMESPACE::sigaddset(&new_set, SIGUSR2);
    ASSERT_EQ(LIBC_NAMESPACE::sigprocmask(SIG_SETMASK, &new_set, nullptr), 0);

    LIBC_NAMESPACE::setcontext(&ctx);
  }

  // Check current mask
  sigset_t current;
  ASSERT_EQ(LIBC_NAMESPACE::sigprocmask(SIG_BLOCK, nullptr, &current), 0);

  // Restore original mask for clean state
  ASSERT_EQ(LIBC_NAMESPACE::sigprocmask(SIG_SETMASK, &old_set, nullptr), 0);

  ASSERT_TRUE(is_signal_set(&current, SIGUSR1));
  ASSERT_FALSE(is_signal_set(&current, SIGUSR2));
}

TEST(LlvmLibcUcontextTest, MakeContextNoArgsAndReturnViaLink) {
  alignas(16) char stack[16384];
  ucontext_t main_ctx;
  ucontext_t ctx;

  g_invoked = 0;
  g_frame_addr = 1;

  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&ctx), 0);
  ctx.uc_stack.ss_sp = stack;
  ctx.uc_stack.ss_size = sizeof(stack);
  ctx.uc_link = &main_ctx;
  LIBC_NAMESPACE::makecontext(&ctx, no_arg_target, 0);

  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&main_ctx), 0);
  if (!g_invoked)
    LIBC_NAMESPACE::setcontext(&ctx);

  EXPECT_EQ(g_invoked, 1);
  EXPECT_EQ(g_frame_addr % STACK_ALIGNMENT, uintptr_t(0));
}

TEST(LlvmLibcUcontextTest, MakeContextArgumentsAndStackAlignment) {
  alignas(16) char stack[16384];
  ucontext_t main_ctx;
  ucontext_t ctx;

  // 6 register arguments.
  g_invoked = 0;
  g_frame_addr = 1;
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&ctx), 0);
  ctx.uc_stack.ss_sp = stack;
  ctx.uc_stack.ss_size = sizeof(stack);
  ctx.uc_link = &main_ctx;
  LIBC_NAMESPACE::makecontext(&ctx,
                              reinterpret_cast<void (*)(void)>(six_arg_target),
                              6, 10, -20, 30, -40, 50, -60);
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&main_ctx), 0);
  if (!g_invoked)
    LIBC_NAMESPACE::setcontext(&ctx);
  EXPECT_EQ(g_args[0], 10);
  EXPECT_EQ(g_args[5], -60);
  EXPECT_EQ(g_frame_addr % STACK_ALIGNMENT, uintptr_t(0));

  // 7 arguments (1 stack argument: odd count tests 16-byte alignment padding).
  g_invoked = 0;
  g_frame_addr = 1;
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&ctx), 0);
  ctx.uc_stack.ss_sp = stack;
  ctx.uc_stack.ss_size = sizeof(stack);
  ctx.uc_link = &main_ctx;
  LIBC_NAMESPACE::makecontext(
      &ctx, reinterpret_cast<void (*)(void)>(seven_arg_target), 7, 1, 2, 3, 4,
      5, 6, -777);
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&main_ctx), 0);
  if (!g_invoked)
    LIBC_NAMESPACE::setcontext(&ctx);
  EXPECT_EQ(g_args[0], 1);
  EXPECT_EQ(g_args[6], -777);
  EXPECT_EQ(g_frame_addr % STACK_ALIGNMENT, uintptr_t(0));

  // 8 arguments (2 stack arguments: even count).
  g_invoked = 0;
  g_frame_addr = 1;
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&ctx), 0);
  ctx.uc_stack.ss_sp = stack;
  ctx.uc_stack.ss_size = sizeof(stack);
  ctx.uc_link = &main_ctx;
  LIBC_NAMESPACE::makecontext(
      &ctx, reinterpret_cast<void (*)(void)>(eight_arg_target), 8, 11, 22, 33,
      44, 55, 66, 77, -88);
  ASSERT_EQ(LIBC_NAMESPACE::getcontext(&main_ctx), 0);
  if (!g_invoked)
    LIBC_NAMESPACE::setcontext(&ctx);
  EXPECT_EQ(g_args[6], 77);
  EXPECT_EQ(g_args[7], -88);
  EXPECT_EQ(g_frame_addr % STACK_ALIGNMENT, uintptr_t(0));
}

TEST(LlvmLibcUcontextTest, MakeContextNullLinkExitsZero) {
  EXPECT_EXITS(
      [] {
        alignas(16) char stack[16384];
        ucontext_t ctx;
        if (LIBC_NAMESPACE::getcontext(&ctx) != 0)
          __builtin_trap();
        ctx.uc_stack.ss_sp = stack;
        ctx.uc_stack.ss_size = sizeof(stack);
        ctx.uc_link = nullptr;
        LIBC_NAMESPACE::makecontext(&ctx, no_arg_target, 0);
        LIBC_NAMESPACE::setcontext(&ctx);
      },
      0);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST(LlvmLibcUcontextTest, MakeContextNullPointerCrash) {
  ASSERT_DEATH([] { LIBC_NAMESPACE::makecontext(nullptr, no_arg_target, 0); },
               WITH_SIGNAL(-1));

  ASSERT_DEATH(
      [] {
        alignas(16) char stack[4096];
        ucontext_t ctx{};
        ctx.uc_stack.ss_sp = stack;
        ctx.uc_stack.ss_size = sizeof(stack);
        LIBC_NAMESPACE::makecontext(&ctx, nullptr, 0);
      },
      WITH_SIGNAL(-1));

  ASSERT_DEATH(
      [] {
        ucontext_t ctx{};
        ctx.uc_stack.ss_sp = nullptr;
        ctx.uc_stack.ss_size = 4096;
        LIBC_NAMESPACE::makecontext(&ctx, no_arg_target, 0);
      },
      WITH_SIGNAL(-1));
}
#endif // defined(LIBC_ADD_NULL_CHECKS)
