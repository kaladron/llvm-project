//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getprotobynumber_r.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_protoent.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/File/file.h"
#include "src/netdb/getproto_internal.h"
#include "src/netdb/getprotobynumber_r.h"
#include "src/stdio/remove.h"
#include "src/string/string_utils.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

namespace {

class ScopedProtocolsFile {
  char path[256];

public:
  ScopedProtocolsFile(const char *file_path,
                      LIBC_NAMESPACE::cpp::string_view content) {
    LIBC_NAMESPACE::internal::strlcpy(path, file_path, sizeof(path));

    LIBC_NAMESPACE::File *file = LIBC_NAMESPACE::openfile(path, "w").value();
    file->write(content.data(), content.size());
    file->close();

    LIBC_NAMESPACE::netdb::set_protocols_path(path);
  }

  ~ScopedProtocolsFile() {
    LIBC_NAMESPACE::netdb::set_protocols_path(nullptr);
    LIBC_NAMESPACE::remove(path);
  }

  const char *get_path() const { return path; }
};

} // namespace

class LlvmLibcGetprotobynumberRTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_protocols_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetprotobynumberRTest, LookupsAndAliases) {
  constexpr LIBC_NAMESPACE::cpp::string_view CONTENT =
      "# leading comment\n"
      "\n"
      "incomplete\n"
      "badnum\txyz\tBAD\n"
      "ip\t0\n"
      "tcp\t6\tTCP\n"
      "sctp\t132\tSCTP StreamControl\n"
      "dup_tcp\t6\tDUP\n";
  ScopedProtocolsFile test_file(
      libc_make_test_file_path("getprotobynumber_r_basic.test"), CONTENT);

  struct protoent entry;
  struct protoent *result = nullptr;
  char buffer[128];

  ASSERT_EQ(LIBC_NAMESPACE::getprotobynumber_r(0, &entry, buffer,
                                               sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "ip");
  EXPECT_EQ(entry.p_proto, 0);
  ASSERT_NE(entry.p_aliases, nullptr);
  EXPECT_EQ(entry.p_aliases[0], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getprotobynumber_r(6, &entry, buffer,
                                               sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "tcp");
  EXPECT_EQ(entry.p_proto, 6);
  ASSERT_NE(entry.p_aliases, nullptr);
  EXPECT_STREQ(entry.p_aliases[0], "TCP");
  EXPECT_EQ(entry.p_aliases[1], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getprotobynumber_r(132, &entry, buffer,
                                               sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "sctp");
  EXPECT_EQ(entry.p_proto, 132);
  ASSERT_NE(entry.p_aliases, nullptr);
  EXPECT_STREQ(entry.p_aliases[0], "SCTP");
  EXPECT_STREQ(entry.p_aliases[1], "StreamControl");
  EXPECT_EQ(entry.p_aliases[2], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getprotobynumber_r(999, &entry, buffer,
                                               sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getprotobynumber_r(-1, &entry, buffer,
                                               sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LlvmLibcGetprotobynumberRTest, BufferOverflowAndOpenErrors) {
  constexpr LIBC_NAMESPACE::cpp::string_view CONTENT = "tcp\t6\tTCP\n";
  ScopedProtocolsFile test_file(
      libc_make_test_file_path("getprotobynumber_r_errors.test"), CONTENT);

  struct protoent entry;
  struct protoent *result = &entry;
  char small_buf[4];

  EXPECT_EQ(LIBC_NAMESPACE::getprotobynumber_r(6, &entry, nullptr, 0, &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getprotobynumber_r(99, &entry, nullptr, 0, &result),
            0);
  EXPECT_EQ(result, nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getprotobynumber_r(6, &entry, small_buf,
                                               sizeof(small_buf), &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  char buffer[64];
  LIBC_NAMESPACE::netdb::set_protocols_path(
      libc_make_test_file_path("getprotobynumber_r_missing.test"));
  EXPECT_EQ(LIBC_NAMESPACE::getprotobynumber_r(6, &entry, buffer,
                                               sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  LIBC_NAMESPACE::netdb::set_protocols_path("/dev/null/not_a_dir");
  EXPECT_EQ(LIBC_NAMESPACE::getprotobynumber_r(6, &entry, buffer,
                                               sizeof(buffer), &result),
            ENOTDIR);
  EXPECT_EQ(result, nullptr);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetprotobynumberRTest, NullptrCrash) {
  struct protoent entry{};
  struct protoent *result = nullptr;
  char buffer[64];

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobynumber_r(6, nullptr, buffer, sizeof(buffer),
                                           &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobynumber_r(6, &entry, nullptr, sizeof(buffer),
                                           &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobynumber_r(6, &entry, buffer, sizeof(buffer),
                                           nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
