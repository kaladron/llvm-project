//===- s01-02-skipping-whitespace.cpp - Kaleidoscope Step 1.2 ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <cctype>
#include <cstdio>

// Returns [0-255] for an ASCII character, or -1 for EOF.
static int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getchar();

  if (LastChar == EOF)
    return -1;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Step 1.2: Whitespace Skipping)\n");
  printf("Type characters with spaces and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != -1)
    printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);

  printf("[EOF]\n");
  return 0;
}
