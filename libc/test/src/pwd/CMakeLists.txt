#===-- CMakeLists.txt ----------------------------------------------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===#

add_custom_target(libc_pwd_unittests)

add_libc_unittest(
  getpwent_test
  SUITE
    libc_pwd_unittests
  SRCS
    getpwent_test.cpp
  DEPENDS
    libc.include.pwd
    libc.hdr.types.struct_passwd
    libc.src.pwd.getpwent
    libc.src.pwd.setpwent
    libc.src.pwd.endpwent
    libc.src.pwd.pwd_utils
    libc.src.__support.File.file
    libc.src.__support.File.platform_file
    libc.src.string.strcmp
)

add_libc_unittest(
  pwd_utils_test
  SUITE
    libc_pwd_unittests
  SRCS
    pwd_utils_test.cpp
  DEPENDS
    libc.hdr.types.struct_passwd
    libc.src.pwd.pwd_utils
)
