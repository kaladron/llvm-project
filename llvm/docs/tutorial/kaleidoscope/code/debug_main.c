//===- debug_main.c - C driver for Kaleidoscope DWARF debugging --*- C -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <stdint.h>
#include <stdio.h>

int32_t factorial(int32_t n);

int main(void) {
  int32_t result = factorial(5);
  printf("factorial(5) = %d\n", result);
  return 0;
}
