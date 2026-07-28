//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "hdr/fcntl_macros.h"
#include "hdr/types/struct_passwd.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/close.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/open.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/unlink.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/write.h"
#include "src/__support/libc_errno.h"
#include "src/pwd/endpwent.h"
#include "src/pwd/getpwent.h"
#include "src/pwd/pwd_utils.h"
#include "src/pwd/setpwent.h"
#include "test/UnitTest/Test.h"

namespace {

class HermeticFile {
  char path[128];

public:
  HermeticFile(const char *name, const char *content) {
    LIBC_NAMESPACE::cpp::StringStream ss(
        LIBC_NAMESPACE::cpp::span<char>(path, sizeof(path)));
    ss << "/tmp/test_" << name << ".txt" << '\0';

    auto fd = LIBC_NAMESPACE::linux_syscalls::open(
        path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd.has_value()) {
      size_t len = 0;
      for (const char *p = content; *p; ++p)
        ++len;
      LIBC_NAMESPACE::linux_syscalls::write(fd.value(), content, len);
      LIBC_NAMESPACE::linux_syscalls::close(fd.value());
    }
  }

  ~HermeticFile() { LIBC_NAMESPACE::linux_syscalls::unlink(path); }

  const char *get_path() const { return path; }
};

} // namespace

TEST(LlvmLibcPwdTest, GetPwentTestSuccess) {
  const char *content = "root:x:0:0:root:/root:/bin/bash\n"
                        "bin:x:1:1:bin:/bin:/sbin/nologin\n";
  HermeticFile test_file("getpwent_success", content);

  LIBC_NAMESPACE::internal::set_passwd_path(test_file.get_path());
  LIBC_NAMESPACE::setpwent();

  struct passwd *pwd1 = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd1 != nullptr);
  ASSERT_STREQ(pwd1->pw_name, "root");
  ASSERT_EQ(pwd1->pw_uid, 0u);

  struct passwd *pwd2 = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd2 != nullptr);
  ASSERT_STREQ(pwd2->pw_name, "bin");
  ASSERT_EQ(pwd2->pw_uid, 1u);

  struct passwd *pwd3 = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd3 == nullptr);

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, GetPwentTestFailure) {
  const char *content = "invalid_line_without_enough_fields\n";
  HermeticFile test_file("getpwent_fail", content);

  LIBC_NAMESPACE::internal::set_passwd_path(test_file.get_path());
  LIBC_NAMESPACE::setpwent();

  libc_errno = 0;
  struct passwd *pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd == nullptr);
  ASSERT_EQ(static_cast<int>(libc_errno), EINVAL);

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, SetPwentTestHermetic) {
  const char *content = "user1:x:1000:1000:User One:/home/user1:/bin/bash\n"
                        "user2:x:1001:1001:User Two:/home/user2:/bin/bash\n";
  HermeticFile test_file("setpwent_hermetic", content);

  LIBC_NAMESPACE::internal::set_passwd_path(test_file.get_path());

  struct passwd *pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd != nullptr);
  ASSERT_STREQ(pwd->pw_name, "user1");

  pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd != nullptr);
  ASSERT_STREQ(pwd->pw_name, "user2");

  // Reset iteration
  LIBC_NAMESPACE::setpwent();

  pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd != nullptr);
  ASSERT_STREQ(pwd->pw_name, "user1");

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, ReopenAfterEndpwent) {
  const char *content = "root:x:0:0:root:/root:/bin/bash\n";
  HermeticFile test_file("reopen_endpwent", content);

  LIBC_NAMESPACE::internal::set_passwd_path(test_file.get_path());

  struct passwd *pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd != nullptr);
  ASSERT_STREQ(pwd->pw_name, "root");

  LIBC_NAMESPACE::endpwent();

  pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd != nullptr);
  ASSERT_STREQ(pwd->pw_name, "root");

  LIBC_NAMESPACE::endpwent();
}

TEST(LlvmLibcPwdTest, FileOpenFailure) {
  LIBC_NAMESPACE::internal::set_passwd_path(
      "/nonexistent_directory/nonexistent_file");
  LIBC_NAMESPACE::endpwent(); // Force close any existing file

  libc_errno = 0;
  struct passwd *pwd = LIBC_NAMESPACE::getpwent();
  ASSERT_TRUE(pwd == nullptr);
  ASSERT_EQ(static_cast<int>(libc_errno), ENOENT);
}
