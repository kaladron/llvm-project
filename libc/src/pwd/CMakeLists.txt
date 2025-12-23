#===-- CMakeLists.txt ----------------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===#

add_entrypoint_object(
  getpwent
  SRCS
    getpwent.cpp
  HDRS
    getpwent.h
  DEPENDS
    libc.include.pwd
    libc.include.llvm-libc-types.struct_passwd
    libc.src.errno.errno
    libc.src.stdio.fopen
    libc.src.stdio.fclose
    libc.src.stdio.fgets
    libc.src.stdlib.atoi
    libc.src.string.strchr
    libc.src.string.strcpy
    libc.src.string.strlen
    libc.src.string.strtok
)

add_entrypoint_object(
  setpwent
  SRCS
    setpwent.cpp
  HDRS
    setpwent.h
  DEPENDS
    libc.include.pwd
    libc.src.stdio.fseek
    libc.hdr.stdio_macros
    .getpwent
)

add_entrypoint_object(
  endpwent
  SRCS
    endpwent.cpp
  HDRS
    endpwent.h
  DEPENDS
    libc.include.pwd
    libc.src.stdio.fclose
    .getpwent
)
