/*===- main.c - C driver for Kaleidoscope AOT ---------------------*- C -*-===*/
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <stdio.h>

double average(double x, double y);
double fib(double n);

int main(void) {
  printf("average(3.0, 4.0) = %f\n", average(3.0, 4.0));
  printf("fib(10.0) = %f\n", fib(10.0));
  return 0;
}
