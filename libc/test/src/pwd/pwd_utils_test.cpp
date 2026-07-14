//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for parse_passwd_line.
///
//===----------------------------------------------------------------------===//

#include "hdr/types/struct_passwd.h"
#include "src/pwd/pwd_utils.h"
#include "test/UnitTest/Test.h"

TEST(LlvmLibcPwdTest, ParsePasswdLine_Success) {
  char line[] = "root:x:0:0:root:/root:/bin/bash";
  struct passwd pwd;
  ASSERT_TRUE(LIBC_NAMESPACE::internal::parse_passwd_line(line, &pwd));
  ASSERT_STREQ(pwd.pw_name, "root");
  ASSERT_STREQ(pwd.pw_passwd, "x");
  ASSERT_EQ(pwd.pw_uid, 0u);
  ASSERT_EQ(pwd.pw_gid, 0u);
  ASSERT_STREQ(pwd.pw_gecos, "root");
  ASSERT_STREQ(pwd.pw_dir, "/root");
  ASSERT_STREQ(pwd.pw_shell, "/bin/bash");
}

TEST(LlvmLibcPwdTest, ParsePasswdLine_EmptyFields) {
  char line[] = "root::0:0::/root:";
  struct passwd pwd;
  ASSERT_TRUE(LIBC_NAMESPACE::internal::parse_passwd_line(line, &pwd));
  ASSERT_STREQ(pwd.pw_name, "root");
  ASSERT_STREQ(pwd.pw_passwd, "");
  ASSERT_EQ(pwd.pw_uid, 0u);
  ASSERT_EQ(pwd.pw_gid, 0u);
  ASSERT_STREQ(pwd.pw_gecos, "");
  ASSERT_STREQ(pwd.pw_dir, "/root");
  ASSERT_STREQ(pwd.pw_shell, "");
}

TEST(LlvmLibcPwdTest, ParsePasswdLine_InvalidNumeric) {
  char line1[] = "root:x:abc:0:root:/root:/bin/bash";
  struct passwd pwd;
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line1, &pwd));

  char line2[] = "root:x:0:def:root:/root:/bin/bash";
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line2, &pwd));
}

TEST(LlvmLibcPwdTest, ParsePasswdLine_MissingFields) {
  char line[] = "root:x:0:0:root:/root"; // Only 6 fields
  struct passwd pwd;
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line, &pwd));
}

TEST(LlvmLibcPwdTest, ParsePasswdLine_NullInput) {
  struct passwd pwd;
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(nullptr, &pwd));
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(nullptr, nullptr));
}

TEST(LlvmLibcPwdTest, ParsePasswdLine_TrailingGarbage) {
  char line1[] = "root:x:0a:0:root:/root:/bin/bash";
  struct passwd pwd;
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line1, &pwd));

  char line2[] = "root:x:0:0b:root:/root:/bin/bash";
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line2, &pwd));
}

TEST(LlvmLibcPwdTest, ParsePasswdLine_Overflow) {
  // 4294967296 is 2^32, which overflows 32-bit unsigned.
  char line1[] = "root:x:4294967296:0:root:/root:/bin/bash";
  struct passwd pwd;
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line1, &pwd));

  char line2[] = "root:x:0:4294967296:root:/root:/bin/bash";
  ASSERT_FALSE(LIBC_NAMESPACE::internal::parse_passwd_line(line2, &pwd));
}
