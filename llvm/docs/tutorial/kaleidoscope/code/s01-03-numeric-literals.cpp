//===- s01-03-numeric-literals.cpp - Kaleidoscope Step 1.3 ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,
  tok_number = -201,
};

static double NumVal; // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise numeric literals: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Step 1.3: Numeric Literals)\n");
  printf("Type math with numbers and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != tok_eof) {
    if (tok == tok_number)
      printf("Token: NUMBER(%f)\n", NumVal);
    else
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
  }

  printf("[EOF]\n");
  return 0;
}
