//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getservbyport_r.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_servent.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/getservbyport_r.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

class LlvmLibcGetservbyportRTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetservbyportRTest, NetworkByteOrderWildcardAndMalformedLines) {
  static constexpr char RAW_DB[] =
      "# leading comment\n"
      "\n"
      "incomplete\n"
      "noslash 80\n"
      "emptyport /tcp\n"
      "emptyproto 80/\n"
      "nondigit xyz/tcp\n"
      "overflow 99999/tcp\n"
      "junk 80x/tcp\n"
      "bigtok "
      "80/1234567890123456789012345678901234567890123456789012345678901234\n"
      "nulserv 80/tcp bad\0alias\n"
      "http 80/tcp www www-http\n"
      "discard 9/sctp";
  const LIBC_NAMESPACE::cpp::string_view CONTENT(RAW_DB, sizeof(RAW_DB) - 1);
  ScopedTestDbFile db(libc_make_test_file_path("getservbyport_r_parse.test"),
                      CONTENT, &LIBC_NAMESPACE::netdb::set_services_path);

  const int PORT_80 =
      static_cast<int>(LIBC_NAMESPACE::net::hton<in_port_t>(80));
  const int PORT_9 = static_cast<int>(LIBC_NAMESPACE::net::hton<in_port_t>(9));

  struct servent entry{};
  struct servent *result = nullptr;
  char buffer[128];

  ASSERT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, "udp", &entry, buffer,
                                            sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, nullptr, &entry, buffer,
                                            sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.s_name, "discard");
  EXPECT_EQ(entry.s_port, PORT_9);
  EXPECT_STREQ(entry.s_proto, "sctp");
  ASSERT_NE(entry.s_aliases, nullptr);
  EXPECT_EQ(entry.s_aliases[0], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_80, "tcp", &entry, buffer,
                                            sizeof(buffer), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.s_name, "http");
  EXPECT_EQ(entry.s_port, PORT_80);
  EXPECT_STREQ(entry.s_proto, "tcp");
  ASSERT_NE(entry.s_aliases, nullptr);
  EXPECT_STREQ(entry.s_aliases[0], "www");
  EXPECT_STREQ(entry.s_aliases[1], "www-http");
  EXPECT_EQ(entry.s_aliases[2], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getservbyport_r(-1, nullptr, &entry, buffer,
                                            sizeof(buffer), &result),
            0);
  EXPECT_EQ(result, nullptr);
}

TEST_F(LlvmLibcGetservbyportRTest, ManyAliasesBufferBoundsAndIoErrors) {
  constexpr LIBC_NAMESPACE::cpp::string_view CONTENT =
      "s 9/t\n"
      "wide 8080/tcp a01 a02 a03 a04 a05 a06 a07 a08 a09 a10 "
      "a11 a12 a13 a14 a15 a16 a17 a18 a19 a20\n";
  ScopedTestDbFile db(libc_make_test_file_path("getservbyport_r_bounds.test"),
                      CONTENT, &LIBC_NAMESPACE::netdb::set_services_path);

  const int PORT_9 = static_cast<int>(LIBC_NAMESPACE::net::hton<in_port_t>(9));
  const int PORT_8080 =
      static_cast<int>(LIBC_NAMESPACE::net::hton<in_port_t>(8080));

  struct servent entry{};
  struct servent *result = &entry;
  alignas(char *) char aligned_buf[512];

  EXPECT_EQ(
      LIBC_NAMESPACE::getservbyport_r(PORT_9, "t", &entry, nullptr, 0, &result),
      ERANGE);
  EXPECT_EQ(result, nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, "t", &entry, aligned_buf, 5,
                                            &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  EXPECT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, "t", &entry, aligned_buf,
                                            sizeof(char *), &result),
            ERANGE);
  EXPECT_EQ(result, nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, "t", &entry, aligned_buf,
                                            2 * sizeof(char *), &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.s_name, "s");
  EXPECT_STREQ(entry.s_proto, "t");

  ASSERT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_8080, "tcp", &entry,
                                            aligned_buf, sizeof(aligned_buf),
                                            &result),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.s_name, "wide");
  EXPECT_STREQ(entry.s_aliases[19], "a20");
  EXPECT_EQ(entry.s_aliases[20], nullptr);

  LIBC_NAMESPACE::netdb::set_services_path(nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::getservbyport_r(-1, nullptr, &entry, aligned_buf,
                                            sizeof(aligned_buf), &result),
            0);
  EXPECT_EQ(result, nullptr);

  LIBC_NAMESPACE::netdb::set_services_path(
      libc_make_test_file_path("getservbyport_r_missing.test"));
  EXPECT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, nullptr, &entry,
                                            aligned_buf, sizeof(aligned_buf),
                                            &result),
            0);
  EXPECT_EQ(result, nullptr);

  LIBC_NAMESPACE::netdb::set_services_path("/dev/null/not_a_dir");
  EXPECT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, nullptr, &entry,
                                            aligned_buf, sizeof(aligned_buf),
                                            &result),
            ENOTDIR);
  EXPECT_EQ(result, nullptr);

  LIBC_NAMESPACE::netdb::set_services_path(".");
  EXPECT_EQ(LIBC_NAMESPACE::getservbyport_r(PORT_9, nullptr, &entry,
                                            aligned_buf, sizeof(aligned_buf),
                                            &result),
            EISDIR);
  EXPECT_EQ(result, nullptr);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetservbyportRTest, NullptrCrash) {
  struct servent entry{};
  struct servent *result = nullptr;
  char buffer[32];

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyport_r(0, nullptr, nullptr, buffer,
                                        sizeof(buffer), &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyport_r(0, nullptr, &entry, nullptr,
                                        sizeof(buffer), &result);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getservbyport_r(0, nullptr, &entry, buffer,
                                        sizeof(buffer), nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
