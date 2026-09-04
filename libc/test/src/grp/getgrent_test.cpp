//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getgrent, setgrent, and endgrent.
///
//===----------------------------------------------------------------------===//

#include "grp_test_utils.h"
#include "hdr/errno_macros.h"
#include "hdr/types/struct_group.h"
#include "src/__support/libc_errno.h"
#include "src/grp/endgrent.h"
#include "src/grp/getgrent.h"
#include "src/grp/grp_utils.h"
#include "src/grp/setgrent.h"
#include "test/UnitTest/Test.h"

TEST_F(LlvmLibcGrpTest, GetGrentTestSuccess) {
  const char *content = "root:x:0:root\n"
                        "bin:x:1:bin,daemon\n"
                        "nogroup:x:65534:\n";
  ScopedGroupFile test_file(libc_make_test_file_path("getgrent_success.test"),
                            content);

  LIBC_NAMESPACE::setgrent();

  struct group *grp1 = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp1, nullptr);
  EXPECT_STREQ(grp1->gr_name, "root");
  EXPECT_STREQ(grp1->gr_passwd, "x");
  EXPECT_EQ(grp1->gr_gid, static_cast<gid_t>(0));
  ASSERT_NE(grp1->gr_mem, nullptr);
  EXPECT_STREQ(grp1->gr_mem[0], "root");
  EXPECT_EQ(grp1->gr_mem[1], nullptr);

  struct group *grp2 = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp2, nullptr);
  EXPECT_STREQ(grp2->gr_name, "bin");
  EXPECT_STREQ(grp2->gr_passwd, "x");
  EXPECT_EQ(grp2->gr_gid, static_cast<gid_t>(1));
  ASSERT_NE(grp2->gr_mem, nullptr);
  EXPECT_STREQ(grp2->gr_mem[0], "bin");
  EXPECT_STREQ(grp2->gr_mem[1], "daemon");
  EXPECT_EQ(grp2->gr_mem[2], nullptr);

  struct group *grp3 = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp3, nullptr);
  EXPECT_STREQ(grp3->gr_name, "nogroup");
  EXPECT_STREQ(grp3->gr_passwd, "x");
  EXPECT_EQ(grp3->gr_gid, static_cast<gid_t>(65534));
  ASSERT_NE(grp3->gr_mem, nullptr);
  EXPECT_EQ(grp3->gr_mem[0], nullptr);

  // POSIX mandates that getgrent shall return null and not change errno on EOF.
  libc_errno = ENOENT;
  struct group *grp4 = LIBC_NAMESPACE::getgrent();
  EXPECT_EQ(grp4, nullptr);
  ASSERT_ERRNO_EQ(ENOENT);

  LIBC_NAMESPACE::endgrent();
}

TEST_F(LlvmLibcGrpTest, GetGrentTestFailure) {
  const char *content = "invalid_line_without_enough_fields\n";
  ScopedGroupFile test_file(libc_make_test_file_path("getgrent_fail.test"),
                            content);

  LIBC_NAMESPACE::setgrent();

  struct group *grp = LIBC_NAMESPACE::getgrent();
  EXPECT_EQ(grp, nullptr);
  ASSERT_ERRNO_EQ(EINVAL);

  LIBC_NAMESPACE::endgrent();
}

TEST_F(LlvmLibcGrpTest, SetGrentTestHermetic) {
  const char *content = "group1:x:1000:user1\n"
                        "group2:x:1001:user2\n";
  ScopedGroupFile test_file(libc_make_test_file_path("setgrent_hermetic.test"),
                            content);

  struct group *grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "group1");

  grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "group2");

  // Reset iteration
  LIBC_NAMESPACE::setgrent();

  grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "group1");

  LIBC_NAMESPACE::endgrent();
}

TEST_F(LlvmLibcGrpTest, ReopenAfterEndgrent) {
  const char *content = "root:x:0:root\n";
  ScopedGroupFile test_file(libc_make_test_file_path("reopen_endgrent.test"),
                            content);

  struct group *grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "root");

  LIBC_NAMESPACE::endgrent();

  grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "root");

  LIBC_NAMESPACE::endgrent();
}

TEST_F(LlvmLibcGrpTest, FileOpenFailure) {
  LIBC_NAMESPACE::grp::TESTONLY_set_group_path(
      "/nonexistent/path/to/group_test_file");

  struct group *grp = LIBC_NAMESPACE::getgrent();
  EXPECT_EQ(grp, nullptr);
  ASSERT_ERRNO_EQ(ENOENT);

  // POSIX specifies that setgrent() sets errno on error.
  libc_errno = 0;
  LIBC_NAMESPACE::setgrent();
  ASSERT_ERRNO_EQ(ENOENT);
}

TEST_F(LlvmLibcGrpTest, BlankLines) {
  const char *content = "\n"
                        "root:x:0:root\n"
                        "\n"
                        "\n"
                        "bin:x:1:bin\n"
                        "\n";
  ScopedGroupFile test_file(libc_make_test_file_path("getgrent_blank.test"),
                            content);

  struct group *grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "root");

  grp = LIBC_NAMESPACE::getgrent();
  ASSERT_NE(grp, nullptr);
  EXPECT_STREQ(grp->gr_name, "bin");

  // POSIX mandates that getgrent shall return null and not change errno on EOF.
  libc_errno = ENOENT;
  grp = LIBC_NAMESPACE::getgrent();
  EXPECT_EQ(grp, nullptr);
  ASSERT_ERRNO_EQ(ENOENT);

  LIBC_NAMESPACE::endgrent();
}
