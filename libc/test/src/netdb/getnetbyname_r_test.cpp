//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getnetbyname_r.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_netent.h"
#include "src/__support/CPP/string_view.h"
#include "src/netdb/getnet_internal.h"
#include "src/netdb/getnetbyname_r.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

class LlvmLibcGetnetbynameRTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_networks_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetnetbynameRTest, DarpaFormatsAndMalformedLines) {
  static constexpr char RAW_DB[] =
      "# leading comment\n"
      "\n"
      "incomplete\n"
      "nondigit xyz\n"
      "emptydot 10..1\n"
      "traildot 10.\n"
      "oct256 256.0.0.0\n"
      "octjunk 10x.0.0.0\n"
      "fivepart 1.2.3.4.5\n"
      "bignum 123456789012345678901234567890123456\n"
      "nulnet 10.0.0.0 bad\0alias\n"
      "loopback 127.0.0.0 loopback-net lo\n"
      "corp 10.1.2\n"
      "campus 172.16\n"
      "class_a 127";
  const LIBC_NAMESPACE::cpp::string_view CONTENT(RAW_DB, sizeof(RAW_DB) - 1);
  ScopedTestDbFile db(libc_make_test_file_path("getnetbyname_r_darpa.test"),
                      CONTENT, &LIBC_NAMESPACE::netdb::set_networks_path);

  struct netent entry{};
  struct netent *result = nullptr;
  char buffer[128];
  int herr = 0;

  static constexpr const char *INVALID_NAMES[] = {
      "missing", "incomplete", "nondigit", "emptydot", "traildot",
      "oct256",  "octjunk",    "fivepart", "bignum",   "nulnet",
  };
  for (const char *bad : INVALID_NAMES) {
    herr = 0;
    result = &entry;
    ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r(bad, &entry, buffer,
                                             sizeof(buffer), &result, &herr),
              0);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(herr, HOST_NOT_FOUND);
  }

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r("lo", &entry, buffer, sizeof(buffer),
                                           &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.n_name, "loopback");
  EXPECT_EQ(entry.n_addrtype, AF_INET);
  EXPECT_EQ(entry.n_net, uint32_t(0x7f000000));
  ASSERT_NE(entry.n_aliases, nullptr);
  EXPECT_STREQ(entry.n_aliases[0], "loopback-net");
  EXPECT_STREQ(entry.n_aliases[1], "lo");
  EXPECT_EQ(entry.n_aliases[2], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r("corp", &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_EQ(entry.n_net, uint32_t(0x000a0102));

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r("campus", &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_EQ(entry.n_net, uint32_t(0x0000ac10));

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r("class_a", &entry, buffer,
                                           sizeof(buffer), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_EQ(entry.n_net, uint32_t(127));
}

TEST_F(LlvmLibcGetnetbynameRTest, ManyAliasesBufferBoundsAndIoErrors) {
  constexpr LIBC_NAMESPACE::cpp::string_view CONTENT =
      "lp 127\n"
      "wide 10.0.0.0 a01 a02 a03 a04 a05 a06 a07 a08 a09 a10 "
      "a11 a12 a13 a14 a15 a16 a17 a18 a19 a20\n";
  ScopedTestDbFile db(libc_make_test_file_path("getnetbyname_r_bounds.test"),
                      CONTENT, &LIBC_NAMESPACE::netdb::set_networks_path);

  struct netent entry{};
  struct netent *result = &entry;
  alignas(char *) char aligned_buf[512];
  int herr = 0;

  EXPECT_EQ(
      LIBC_NAMESPACE::getnetbyname_r("lp", &entry, nullptr, 0, &result, &herr),
      ERANGE);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);

  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname_r("lp", &entry, aligned_buf, 4,
                                           &result, &herr),
            ERANGE);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);

  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname_r("lp", &entry, aligned_buf,
                                           sizeof(char *), &result, &herr),
            ERANGE);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r("lp", &entry, aligned_buf,
                                           2 * sizeof(char *), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.n_name, "lp");

  ASSERT_EQ(LIBC_NAMESPACE::getnetbyname_r("a20", &entry, aligned_buf,
                                           sizeof(aligned_buf), &result, &herr),
            0);
  ASSERT_EQ(result, &entry);
  EXPECT_STREQ(entry.n_name, "wide");
  EXPECT_STREQ(entry.n_aliases[19], "a20");
  EXPECT_EQ(entry.n_aliases[20], nullptr);

  LIBC_NAMESPACE::netdb::set_networks_path(nullptr);
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname_r("__llvm_libc_nonexistent_net__",
                                           &entry, aligned_buf,
                                           sizeof(aligned_buf), &result, &herr),
            0);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  LIBC_NAMESPACE::netdb::set_networks_path(
      libc_make_test_file_path("getnetbyname_r_missing.test"));
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname_r("lp", &entry, aligned_buf,
                                           sizeof(aligned_buf), &result, &herr),
            0);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  LIBC_NAMESPACE::netdb::set_networks_path("/dev/null/not_a_dir");
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname_r("lp", &entry, aligned_buf,
                                           sizeof(aligned_buf), &result, &herr),
            ENOTDIR);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);

  LIBC_NAMESPACE::netdb::set_networks_path(".");
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::getnetbyname_r("lp", &entry, aligned_buf,
                                           sizeof(aligned_buf), &result, &herr),
            EISDIR);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(herr, NO_RECOVERY);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetnetbynameRTest, NullptrCrash) {
  struct netent entry{};
  struct netent *result = nullptr;
  char buffer[32];
  int herr = 0;

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyname_r(nullptr, &entry, buffer, sizeof(buffer),
                                       &result, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyname_r("lp", nullptr, buffer, sizeof(buffer),
                                       &result, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyname_r("lp", &entry, nullptr, sizeof(buffer),
                                       &result, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyname_r("lp", &entry, buffer, sizeof(buffer),
                                       nullptr, &herr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::getnetbyname_r("lp", &entry, buffer, sizeof(buffer),
                                       &result, nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
