//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for gethostbyname_r and gethostbyname.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_hostent.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/netdb/h_errno.h"
#include "src/__support/netdb/hosts.h"
#include "src/netdb/gethostbyname.h"
#include "src/netdb/gethostbyname_r.h"
#include "src/string/string_utils.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

namespace {

constexpr void make_alias_name(char (&out)[16], size_t index) {
  constexpr LIBC_NAMESPACE::cpp::string_view PREFIX = "host_alias_";
  for (size_t i = 0; i < PREFIX.size(); ++i)
    out[i] = PREFIX[i];
  out[PREFIX.size()] = static_cast<char>('0' + (index / 10));
  out[PREFIX.size() + 1] = static_cast<char>('0' + (index % 10));
  out[PREFIX.size() + 2] = '\0';
}

} // namespace

class LlvmLibcGethostbynameTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGethostbynameTest, LiteralHostsTwentyAliasesAndBounds) {
  using LIBC_NAMESPACE::cpp::string_view;

  struct hostent he{};
  struct hostent *res = &he;
  alignas(char *) char buf[1024];
  int herr = 0;

  // Exercise default nullptr hosts_path with RFC 6761 localhost synthesis.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("hermetic-probe.localhost", &he,
                                            buf, sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "hermetic-probe.localhost");
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[0]), uint8_t(127));
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[3]), uint8_t(1));

  // Missing /etc/hosts file returns 0 + HOST_NOT_FOUND cleanly.
  LIBC_NAMESPACE::netdb::set_hosts_path(
      libc_make_test_file_path("host_r_missing.test"));
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("missing.local", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  constexpr size_t TWENTY_ALIASES = 20;
  char db[2400];
  size_t pos = 0;
  auto append = [&](string_view s) {
    for (char ch : s)
      db[pos++] = ch;
  };

  // Non-matching >512B line before matches, IPv6-only record, multi-homed IPv4.
  append("10.0.0.1 ");
  for (size_t i = 0; i < 40; ++i)
    append("other_padding ");
  append("other.local\n");
  append("::1 v6.local\n");
  append("::1 app.local app\n");
  append("10.9.8.7 app.local app a1\n");
  append("10.9.8.8 app.local app\n");

  // 20-alias >512B /etc/hosts line.
  append("10.2.0.1 wide20.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char alias[16];
    make_alias_name(alias, i);
    db[pos++] = ' ';
    append(string_view(alias, 13));
  }
  for (size_t i = 0; i < 20; ++i)
    append("        ");
  db[pos++] = '\n';

  // >16 multi-homed IPv4 addresses cap.
  for (size_t i = 0; i < 18; ++i)
    append("10.3.0.1 multi18.local\n");

  ScopedTestDbFile hdb(libc_make_test_file_path("host_r.test"),
                       string_view(db, pos),
                       &LIBC_NAMESPACE::netdb::set_hosts_path);

  // Dotted-quad IPv4 literal with nullable h_errnop and ERANGE bounds.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("192.0.2.55", &he, buf, sizeof(buf),
                                            &res, nullptr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "192.0.2.55");
  EXPECT_EQ(he.h_addrtype, AF_INET);
  EXPECT_EQ(he.h_length, 4);
  EXPECT_EQ(he.h_aliases[0], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[3]), uint8_t(55));
  EXPECT_EQ(he.h_addr_list[1], nullptr);

  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("192.0.2.55", &he, nullptr, 0, &res,
                                            &herr),
            ERANGE);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  herr = 0;
  EXPECT_EQ(
      LIBC_NAMESPACE::gethostbyname_r("192.0.2.55", &he, buf, 16, &res, &herr),
      ERANGE);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  // Multi-homed /etc/hosts match preserves first-line aliases and both IPs.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("APP", &he, buf, sizeof(buf), &res,
                                            &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "app.local");
  EXPECT_STREQ(he.h_aliases[0], "app");
  EXPECT_STREQ(he.h_aliases[1], "a1");
  EXPECT_EQ(he.h_aliases[2], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[0][3]), uint8_t(7));
  ASSERT_NE(he.h_addr_list[1], nullptr);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr_list[1][3]), uint8_t(8));
  EXPECT_EQ(he.h_addr_list[2], nullptr);

  // Sweep every buffer capacity from 0..80 to exercise text, padding, ptrs,
  // and address ERANGE boundaries without modifying errno.
  EXPECT_EQ(
      LIBC_NAMESPACE::gethostbyname_r("app", &he, nullptr, 0, &res, nullptr),
      ERANGE);
  for (size_t cap = 1; cap < 80; ++cap) {
    herr = 0;
    const int RC =
        LIBC_NAMESPACE::gethostbyname_r("app", &he, buf, cap, &res, &herr);
    if (RC != 0) {
      EXPECT_EQ(RC, ERANGE);
      EXPECT_EQ(res, nullptr);
      EXPECT_EQ(herr, NETDB_INTERNAL);
    }
  }

  // 20-alias >512B record resolves last alias and returns all 20 aliases.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("HOST_ALIAS_19", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "wide20.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char expected[16];
    make_alias_name(expected, i);
    ASSERT_NE(he.h_aliases[i], nullptr);
    EXPECT_STREQ(he.h_aliases[i], expected);
  }
  EXPECT_EQ(he.h_aliases[TWENTY_ALIASES], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("multi18.local", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_EQ(he.h_addr_list[16], nullptr);

  ASSERT_EQ(LIBC_NAMESPACE::gethostbyname_r("api.localhost.", &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  EXPECT_STREQ(he.h_name, "api.localhost");

  struct hostent *tls = LIBC_NAMESPACE::gethostbyname("host_alias_19");
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "wide20.local");
  EXPECT_STREQ(tls->h_aliases[19], "host_alias_19");
  EXPECT_EQ(tls->h_aliases[20], nullptr);

  tls = LIBC_NAMESPACE::gethostbyname("api.localhost.");
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "api.localhost");
  EXPECT_EQ(static_cast<uint8_t>(tls->h_addr[0]), uint8_t(127));

  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname("192.168.1.999"), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), HOST_NOT_FOUND);

  static constexpr const char *MISS_CASES[] = {
      "bad.invalid",
      "v6.local",
      "1.2.3.999",
  };
  for (const char *miss : MISS_CASES) {
    herr = 0;
    EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r(miss, &he, buf, sizeof(buf), &res,
                                              &herr),
              0);
    EXPECT_EQ(res, nullptr);
    EXPECT_EQ(herr, HOST_NOT_FOUND);
  }
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("v6.local", &he, buf, sizeof(buf),
                                            &res, nullptr),
            0);

  // Open failure (ENOTDIR) and read failure (EISDIR) set NO_RECOVERY.
  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, hdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/hosts",
                                    sizeof(notdir_path) - BASE_LEN);
  LIBC_NAMESPACE::netdb::set_hosts_path(notdir_path);
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("app", &he, buf, sizeof(buf), &res,
                                            &herr),
            ENOTDIR);
  EXPECT_EQ(herr, NO_RECOVERY);

  LIBC_NAMESPACE::netdb::set_hosts_path(".");
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyname_r("app", &he, buf, sizeof(buf), &res,
                                            &herr),
            EISDIR);
  EXPECT_EQ(herr, NO_RECOVERY);
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGethostbynameTest, NullptrCrash) {
  struct hostent he{};
  struct hostent *res = nullptr;
  char buf[16];

  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r(nullptr, &he, buf, sizeof(buf), &res,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r("a", nullptr, buf, sizeof(buf), &res,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r("a", &he, nullptr, sizeof(buf), &res,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyname_r("a", &he, buf, sizeof(buf), nullptr,
                                        nullptr);
      },
      WITH_SIGNAL(-1));
  ASSERT_DEATH([] { LIBC_NAMESPACE::gethostbyname(nullptr); }, WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
