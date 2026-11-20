//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getprotobyname_r.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_protoent.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/File/file.h"
#include "src/__support/c_string.h"
#include "src/netdb/getproto_internal.h"
#include "src/netdb/getprotobyname_r.h"
#include "src/stdio/remove.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"

namespace {

class ScopedProtocolsFile {
  LIBC_NAMESPACE::CString path;

public:
  ScopedProtocolsFile(const ScopedProtocolsFile &) = delete;
  ScopedProtocolsFile &operator=(const ScopedProtocolsFile &) = delete;

  ScopedProtocolsFile(const char *file_name,
                      LIBC_NAMESPACE::cpp::string_view content)
      : path(libc_make_test_file_path(file_name)) {
    LIBC_NAMESPACE::File *f = LIBC_NAMESPACE::openfile(path, "w").value();
    f->write(content.data(), content.size());
    f->close();
    LIBC_NAMESPACE::netdb::set_protocols_path(path);
  }

  ~ScopedProtocolsFile() {
    LIBC_NAMESPACE::netdb::set_protocols_path(nullptr);
    LIBC_NAMESPACE::remove(path);
  }
};

constexpr size_t align_up(size_t offset, size_t align) {
  return ((offset + align - 1) / align) * align;
}

} // namespace

class LlvmLibcGetprotobynameRTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_protocols_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetprotobynameRTest, CanonicalAndAliasLookups) {
  constexpr LIBC_NAMESPACE::cpp::string_view CONTENT =
      "# Long comment banner exceeding the 128-byte caller buffer: "
      "0123456789abcdef0123456789abcdef0123456789abcdef"
      "0123456789abcdef0123456789abcdef\n"
      "\n"
      "   \t  \r\n"
      "ip\t0\tIP\t# internet protocol\n"
      "tcp\t6\tTCP\r\n"
      "udp\t17\tUDP#adjacent comment\n"
      "gre\t47\n"
      "sctp\t132\tSCTP StreamControl sctp-alt\n"
      "primary\t200\tshadowed\n"
      "shadowed\t201\tSHADOWED\n"
      "raw\t255";
  ScopedProtocolsFile test_file("getprotobyname_r_aliases.test", CONTENT);

  struct protoent entry{};
  char buffer[128];
  struct protoent *result = nullptr;

  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("tcp", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "tcp");
  EXPECT_EQ(entry.p_proto, 6);
  ASSERT_NE(entry.p_aliases, nullptr);
  ASSERT_STREQ(entry.p_aliases[0], "TCP");
  EXPECT_EQ(entry.p_aliases[1], nullptr);

  // Returned strings and pointer array live inside the caller buffer.
  char *begin = buffer;
  char *end = buffer + sizeof(buffer);
  EXPECT_GE(entry.p_name, begin);
  EXPECT_LT(entry.p_name, end);
  EXPECT_GE(reinterpret_cast<char *>(entry.p_aliases), begin);
  EXPECT_LT(reinterpret_cast<char *>(entry.p_aliases), end);
  EXPECT_GE(entry.p_aliases[0], begin);
  EXPECT_LT(entry.p_aliases[0], end);

  // Alias lookup with an immediately adjacent '#' comment.
  result = nullptr;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("UDP", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "udp");
  EXPECT_EQ(entry.p_proto, 17);
  ASSERT_NE(entry.p_aliases, nullptr);
  ASSERT_STREQ(entry.p_aliases[0], "UDP");
  EXPECT_EQ(entry.p_aliases[1], nullptr);

  // Multiple aliases on one entry.
  result = nullptr;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("sctp-alt", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "sctp");
  EXPECT_EQ(entry.p_proto, 132);
  ASSERT_NE(entry.p_aliases, nullptr);
  ASSERT_STREQ(entry.p_aliases[0], "SCTP");
  ASSERT_STREQ(entry.p_aliases[1], "StreamControl");
  ASSERT_STREQ(entry.p_aliases[2], "sctp-alt");
  EXPECT_EQ(entry.p_aliases[3], nullptr);

  // Earlier alias match takes precedence over a later canonical name.
  result = nullptr;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("shadowed", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "primary");
  EXPECT_EQ(entry.p_proto, 200);

  // Zero-alias entry at EOF without a trailing newline.
  result = nullptr;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("raw", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "raw");
  EXPECT_EQ(entry.p_proto, 255);
  ASSERT_NE(entry.p_aliases, nullptr);
  EXPECT_EQ(entry.p_aliases[0], nullptr);

  // Strict prefixes, suffixes, empty names, and absent entries return 0 with
  // a null result pointer.
  result = &entry;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("tc", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  result = &entry;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("tcp6", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  result = &entry;
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("", &entry, buffer, sizeof(buffer),
                                             &result),
            0);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LlvmLibcGetprotobynameRTest, MalformedAndEmbeddedNulLines) {
  static constexpr char CONTENT[] = "tcp\n"
                                    "tcp   \n"
                                    "tcp /\n"
                                    "tcp -1\n"
                                    "tcp 6x TCP\n"
                                    "tcp 999999999999\n"
                                    "\0tcp 6\n"
                                    "tc\0p 6\n"
                                    "tcp 6 TCP bad\0alias\n"
                                    "tcp 6 TCP # inline \0 comment\n";
  ScopedProtocolsFile test_file(
      "getprotobyname_r_malformed.test",
      LIBC_NAMESPACE::cpp::string_view(CONTENT, sizeof(CONTENT) - 1));

  struct protoent entry{};
  char buffer[128];
  struct protoent *result = nullptr;

  // All malformed and pre-comment NUL lines are skipped; the valid final line
  // (whose NUL lies inside the '#' comment) matches.
  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("TCP", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "tcp");
  EXPECT_EQ(entry.p_proto, 6);
  ASSERT_NE(entry.p_aliases, nullptr);
  ASSERT_STREQ(entry.p_aliases[0], "TCP");
  EXPECT_EQ(entry.p_aliases[1], nullptr);
}

TEST_F(LlvmLibcGetprotobynameRTest, BufferTooSmallReturnsERange) {
  constexpr LIBC_NAMESPACE::cpp::string_view CONTENT = "ip 0\n"
                                                       "tcp 6 TCP\n";
  ScopedProtocolsFile test_file("getprotobyname_r_erange.test", CONTENT);

  struct protoent entry{};
  struct protoent *result = &entry;
  alignas(char *) char aligned_buf[64];

  // Zero-length (with nullptr) and 1-byte buffers return ERANGE without
  // crashing.
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("ip", &entry, nullptr, 0, &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  result = &entry;
  EXPECT_EQ(
      LIBC_NAMESPACE::getprotobyname_r("ip", &entry, aligned_buf, 1, &result),
      ERANGE);
  EXPECT_EQ(result, nullptr);

  // Truncated record line ("ip 0" needs 5 bytes including NUL).
  result = &entry;
  EXPECT_EQ(
      LIBC_NAMESPACE::getprotobyname_r("ip", &entry, aligned_buf, 4, &result),
      ERANGE);
  EXPECT_EQ(result, nullptr);

  // Record fits (5 bytes), but scratch is smaller than alignment padding.
  result = &entry;
  EXPECT_EQ(
      LIBC_NAMESPACE::getprotobyname_r("ip", &entry, aligned_buf, 5, &result),
      ERANGE);
  EXPECT_EQ(result, nullptr);

  // Alignment padding fits, but terminating nullptr in p_aliases does not.
  constexpr size_t IP_EXACT = align_up(5, alignof(char *)) + sizeof(char *);
  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("ip", &entry, aligned_buf,
                                             IP_EXACT - 1, &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("ip", &entry, aligned_buf,
                                             IP_EXACT, &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "ip");
  ASSERT_NE(entry.p_aliases, nullptr);
  EXPECT_EQ(entry.p_aliases[0], nullptr);

  // Alias pointer array boundary for "tcp 6 TCP" (10 bytes + padding +
  // 2 * sizeof(char *)).
  constexpr size_t TCP_EXACT =
      align_up(10, alignof(char *)) + 2 * sizeof(char *);
  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("TCP", &entry, aligned_buf,
                                             TCP_EXACT - 1, &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getprotobyname_r("TCP", &entry, aligned_buf,
                                             TCP_EXACT, &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.p_name, "tcp");
  ASSERT_NE(entry.p_aliases, nullptr);
  ASSERT_STREQ(entry.p_aliases[0], "TCP");
  EXPECT_EQ(entry.p_aliases[1], nullptr);
}

TEST_F(LlvmLibcGetprotobynameRTest, MissingFileAndIoErrors) {
  struct protoent entry{};
  char buffer[128];
  struct protoent *result = &entry;

  // Missing database file returns 0 with a null result pointer.
  const auto MISSING_PATH =
      libc_make_test_file_path("getprotobyname_r_missing.test");
  LIBC_NAMESPACE::netdb::set_protocols_path(MISSING_PATH);
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("tcp", &entry, buffer,
                                             sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  // Path traversing through a regular file fails openfile with ENOTDIR.
  ScopedProtocolsFile regular_file("getprotobyname_r_file.test",
                                   "hermetic_only_proto 199\n");
  const auto NOTDIR_PATH =
      libc_make_test_file_path("getprotobyname_r_file.test/protocols");
  LIBC_NAMESPACE::netdb::set_protocols_path(NOTDIR_PATH);

  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("tcp", &entry, buffer,
                                             sizeof(buffer), &result),
            ENOTDIR);
  EXPECT_EQ(result, nullptr);

  // Reading a directory fails read_line with EISDIR.
  LIBC_NAMESPACE::netdb::set_protocols_path(".");
  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("tcp", &entry, buffer,
                                             sizeof(buffer), &result),
            EISDIR);
  EXPECT_EQ(result, nullptr);

  // Resetting to nullptr restores the default database path.
  LIBC_NAMESPACE::netdb::set_protocols_path(nullptr);
  result = &entry;
  EXPECT_EQ(LIBC_NAMESPACE::getprotobyname_r("hermetic_only_proto", &entry,
                                             buffer, sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetprotobynameRTest, NullPointerCrash) {
  struct protoent entry{};
  char buffer[32];
  struct protoent *result = nullptr;

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobyname_r(nullptr, &entry, buffer,
                                         sizeof(buffer), &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobyname_r("tcp", nullptr, buffer, sizeof(buffer),
                                         &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobyname_r("tcp", &entry, nullptr, sizeof(buffer),
                                         &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getprotobyname_r("tcp", &entry, buffer, sizeof(buffer),
                                         nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
