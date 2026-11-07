//===- s01-01-first-token.cpp - Kaleidoscope Step 1.1 -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <cstdio>

int main() {
  printf("Kaleidoscope Token Inspector (Step 1.1: Single Characters)\n");
  printf("Type characters and press Enter (Ctrl+D to exit):\n\n");

  int c;
  while ((c = getchar()) != EOF) {
    if (c != '\n' && c != '\r')
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(c), c);
  }

  printf("[EOF]\n");
  return 0;
}
