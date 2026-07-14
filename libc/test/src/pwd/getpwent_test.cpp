//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for getpwent.
///
//===----------------------------------------------------------------------===//

#include "hdr/types/struct_passwd.h"
#include "src/__support/File/file.h"
#include "src/__support/libc_errno.h"
#include "src/pwd/endpwent.h"
#include "src/pwd/getpwent.h"
#include "src/pwd/pwd_utils.h"
#include "src/pwd/setpwent.h"
#include "test/UnitTest/Test.h"

static const char *valid_passwd_data =
    "root:x:0:0:root:/root:/bin/bash\n"
    "daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\n"
    "user2:x:3:3:user2:/home/user2:/bin/sh\n";

static const char *bad_passwd_data = "root:x:0:0:root:/root:/bin/bash\n"
                                     "baduser:x:invalid:invalid:::\n"
                                     "user2:x:3:3:user2:/home/user2:/bin/sh\n";

static bool create_test_file(const char *path, const char *data) {
  auto result = LIBC_NAMESPACE::openfile(path, "w");
  if (!result.has_value())
    return false;
  auto f = result.value();
  size_t len = 0;
  while (data[len] != '\0')
    len++;
  auto write_result = f->write(data, len);
  if (write_result.value != len) {
    f->close();
    return false;
  }
  f->close();
  return true;
}

TEST(LlvmLibcPwdTest, GetPwentTestSuccess) {
  auto TEST_FILE = libc_make_test_file_path("getpwent_success.test");
  ASSERT_TRUE(create_test_file(TEST_FILE, valid_passwd_data));

  LIBC_NAMESPACE::internal::set_passwd_path(TEST_FILE);

  // First entry: root
  struct passwd *pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw != nullptr);
  EXPECT_STREQ(pw->pw_name, "root");
  EXPECT_STREQ(pw->pw_passwd, "x");
  EXPECT_EQ(pw->pw_uid, 0u);
  EXPECT_EQ(pw->pw_gid, 0u);
  EXPECT_STREQ(pw->pw_gecos, "root");
  EXPECT_STREQ(pw->pw_dir, "/root");
  EXPECT_STREQ(pw->pw_shell, "/bin/bash");

  // Second entry: daemon
  pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw != nullptr);
  EXPECT_STREQ(pw->pw_name, "daemon");
  EXPECT_EQ(pw->pw_uid, 1u);

  // Third entry: user2
  pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw != nullptr);
  EXPECT_STREQ(pw->pw_name, "user2");
  EXPECT_EQ(pw->pw_uid, 3u);

  // End of file
  pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw == nullptr);

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, GetPwentTestFailure) {
  auto TEST_FILE = libc_make_test_file_path("getpwent_failure.test");
  ASSERT_TRUE(create_test_file(TEST_FILE, bad_passwd_data));

  LIBC_NAMESPACE::internal::set_passwd_path(TEST_FILE);

  // First entry: root (valid)
  struct passwd *pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw != nullptr);
  EXPECT_STREQ(pw->pw_name, "root");

  // Second entry: baduser (invalid) -> should fail
  libc_errno = 0;
  pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw == nullptr);
  EXPECT_EQ(static_cast<int>(libc_errno), EINVAL);

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, SetPwentTestHermetic) {
  auto TEST_FILE = libc_make_test_file_path("setpwent_hermetic.test");
  ASSERT_TRUE(create_test_file(TEST_FILE, valid_passwd_data));

  LIBC_NAMESPACE::internal::set_passwd_path(TEST_FILE);

  // Read first entry
  LIBC_NAMESPACE::setpwent();
  struct passwd *pw1 = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw1 != nullptr);

  // Copy pw1->pw_name
  char pw1_name[256];
  size_t i = 0;
  for (; pw1->pw_name[i] != '\0' && i < sizeof(pw1_name) - 1; ++i) {
    pw1_name[i] = pw1->pw_name[i];
  }
  pw1_name[i] = '\0';

  // Read second entry
  struct passwd *pw2 = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw2 != nullptr);
  EXPECT_STRNE(pw1_name, pw2->pw_name); // Should be different

  // Rewind
  LIBC_NAMESPACE::setpwent();
  struct passwd *pw3 = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw3 != nullptr);
  EXPECT_STREQ(pw1_name, pw3->pw_name); // Should be first entry again

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, GetPwentTruncationTest) {
  auto TEST_FILE = libc_make_test_file_path("getpwent_truncation.test");

  char test_data[2048];
  size_t idx = 0;

  // "longuser:x:4:4:"
  const char *prefix = "longuser:x:4:4:";
  for (size_t i = 0; prefix[i] != '\0'; ++i)
    test_data[idx++] = prefix[i];

  // 1100 'a's (gecos field) - triggers truncation (>1023 chars)
  for (int i = 0; i < 1100; ++i)
    test_data[idx++] = 'a';

  // ":/home/longuser:/bin/sh\n"
  const char *suffix = ":/home/longuser:/bin/sh\n";
  for (size_t i = 0; suffix[i] != '\0'; ++i)
    test_data[idx++] = suffix[i];

  test_data[idx] = '\0';

  ASSERT_TRUE(create_test_file(TEST_FILE, test_data));
  LIBC_NAMESPACE::internal::set_passwd_path(TEST_FILE);

  // The long line should be truncated and cause failure
  libc_errno = 0;
  struct passwd *pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw == nullptr);
  EXPECT_EQ(static_cast<int>(libc_errno), EINVAL);

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, ReopenAfterEndpwent) {
  auto TEST_FILE = libc_make_test_file_path("getpwent_reopen.test");
  ASSERT_TRUE(create_test_file(TEST_FILE, valid_passwd_data));
  LIBC_NAMESPACE::internal::set_passwd_path(TEST_FILE);

  // Read first entry
  struct passwd *pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw != nullptr);
  EXPECT_STREQ(pw->pw_name, "root");

  // Close database
  LIBC_NAMESPACE::endpwent();

  // Read again -> should reopen and start from root again
  pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw != nullptr);
  EXPECT_STREQ(pw->pw_name, "root");

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, FileOpenFailure) {
  LIBC_NAMESPACE::internal::set_passwd_path("/nonexistent_file_pwd_test");

  libc_errno = 0;
  struct passwd *pw = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pw == nullptr);
  EXPECT_EQ(static_cast<int>(libc_errno), ENOENT);
}
