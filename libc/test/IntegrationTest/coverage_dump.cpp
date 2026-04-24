//===-- coverage_dump.cpp - Write profile data via raw syscalls -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Lightweight profile data writer for hermetic tests. Uses the profile
// runtime's buffer API (__llvm_profile_write_buffer) and writes via direct
// Linux syscalls through inline assembly, avoiding all dependencies on
// stdio/libc functions that the hermetic environment may not provide.
//
// This file MUST be compiled WITHOUT coverage instrumentation
// (-fno-profile-instr-generate -fno-coverage-mapping) to avoid circular
// dependencies and to prevent TLS access before initialization.
//
//===----------------------------------------------------------------------===//

using uint64_t = unsigned long long;

// Profile runtime buffer API (from compiler-rt InstrProfiling.h).
extern "C" {
uint64_t __llvm_profile_get_size_for_buffer(void);
int __llvm_profile_write_buffer(char *Buffer);
}

namespace {

// Raw x86_64 Linux syscall via inline assembly.
inline long raw_syscall3(long number, long arg1, long arg2, long arg3) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
                   : "rcx", "r11", "memory");
  return ret;
}

inline long raw_syscall6(long number, long arg1, long arg2, long arg3,
                         long arg4, long arg5, long arg6) {
  long ret;
  register long r10 __asm__("r10") = arg4;
  register long r8 __asm__("r8") = arg5;
  register long r9 __asm__("r9") = arg6;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3), "r"(r10),
                     "r"(r8), "r"(r9)
                   : "rcx", "r11", "memory");
  return ret;
}

// Linux syscall numbers for x86_64.
constexpr long SYS_write = 1;
constexpr long SYS_open = 2;
constexpr long SYS_close = 3;
constexpr long SYS_mmap = 9;
constexpr long SYS_munmap = 11;

// File open flags.
constexpr int O_WRONLY = 01;
constexpr int O_CREAT = 0100;
constexpr int O_TRUNC = 01000;

// mmap constants.
constexpr int PROT_READ = 1;
constexpr int PROT_WRITE = 2;
constexpr int MAP_PRIVATE = 0x02;
constexpr int MAP_ANONYMOUS = 0x20;

// Write all bytes, handling partial writes.
bool write_all(int fd, const char *buf, uint64_t size) {
  while (size > 0) {
    long written =
        raw_syscall3(SYS_write, fd, reinterpret_cast<long>(buf), size);
    if (written <= 0)
      return false;
    buf += written;
    size -= written;
  }
  return true;
}

// Dump coverage profile data at program exit.
// This is placed in .fini_array so it runs during exit() processing,
// after main() returns but before the process terminates.
void dump_profile_data() {
  uint64_t size = __llvm_profile_get_size_for_buffer();
  if (size == 0)
    return;

  const char *filename = "default.profraw";

  int fd = raw_syscall3(SYS_open, reinterpret_cast<long>(filename),
                        O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    return;

  // Use mmap to allocate memory for the profile buffer.
  long addr =
      raw_syscall6(SYS_mmap, 0, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (addr > 0) {
    __llvm_profile_write_buffer(reinterpret_cast<char *>(addr));
    write_all(fd, reinterpret_cast<const char *>(addr), size);
    raw_syscall3(SYS_munmap, addr, size, 0);
  }

  raw_syscall3(SYS_close, fd, 0, 0);
}

// Register dump_profile_data in .fini_array so it runs at exit.
__attribute__((used, section(".fini_array"))) void (*coverage_dump_fini)() =
    dump_profile_data;

} // anonymous namespace
