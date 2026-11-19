//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for gethostbyaddr_r and gethostbyaddr.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_hostent.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/h_errno.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/netdb/gethostbyaddr.h"
#include "src/netdb/gethostbyaddr_r.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/string_utils.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

namespace {

constexpr void make_rev_alias(char (&out)[16], size_t index) {
  constexpr LIBC_NAMESPACE::cpp::string_view PREFIX = "rev_alias_";
  for (size_t i = 0; i < PREFIX.size(); ++i)
    out[i] = PREFIX[i];
  out[PREFIX.size()] = static_cast<char>('0' + (index / 10));
  out[PREFIX.size() + 1] = static_cast<char>('0' + (index % 10));
  out[PREFIX.size() + 2] = '\0';
}

} // namespace

class LlvmLibcGethostbyaddrTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::set_dns_wire_hook(nullptr);
    LIBC_NAMESPACE::netdb::set_hosts_path(nullptr);
    LIBC_NAMESPACE::netdb::set_resolv_conf_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGethostbyaddrTest, HostsReverseTwentyAliasesV4MappedAndBounds) {
  using LIBC_NAMESPACE::cpp::span;
  using LIBC_NAMESPACE::cpp::string_view;

  ScopedTestDbFile rdb(libc_make_test_file_path("hba_resolv.test"),
                       "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n",
                       &LIBC_NAMESPACE::netdb::set_resolv_conf_path);
  LIBC_NAMESPACE::netdb::set_dns_wire_hook(
      [](const LIBC_NAMESPACE::netdb::ScopedIpAddr &, bool,
         span<const uint8_t> q, span<uint8_t> r) -> int {
        using namespace LIBC_NAMESPACE::netdb;
        LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), q.size());
        write_u16_be(r, 2, DNS_FLAG_QR | DNS_RCODE_NXDOMAIN);
        return static_cast<int>(q.size());
      });

  struct hostent he{};
  struct hostent *res = &he;
  alignas(char *) char buf[1024];
  int herr = 0;

  static constexpr uint8_t ZERO4[4] = {0, 0, 0, 0};
  static constexpr uint8_t APP4[4] = {10, 9, 8, 7};
  static constexpr uint8_t WIDE4[4] = {10, 2, 0, 1};
  static constexpr uint8_t NUL4[4] = {10, 2, 0, 9};

  // Default nullptr hosts_path + all-zero IPv4 returns HOST_NOT_FOUND without
  // querying DNS or formatting numeric IP.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(ZERO4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  // Missing /etc/hosts file returns 0 + HOST_NOT_FOUND cleanly.
  LIBC_NAMESPACE::netdb::set_hosts_path(
      libc_make_test_file_path("hba_missing.test"));
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(APP4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, HOST_NOT_FOUND);

  // Unsupported family and mismatched length validation.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(APP4, 4, AF_UNIX, &he, buf,
                                            sizeof(buf), &res, &herr),
            EAFNOSUPPORT);
  EXPECT_EQ(herr, NO_RECOVERY);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(APP4, 8, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            EINVAL);
  EXPECT_EQ(herr, NO_RECOVERY);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(APP4, 4, AF_INET6, &he, buf,
                                            sizeof(buf), &res, nullptr),
            EINVAL);

  constexpr size_t TWENTY_ALIASES = 20;
  char db[2048];
  size_t pos = 0;
  auto append = [&](string_view s) {
    for (char ch : s)
      db[pos++] = ch;
  };

  append("# reverse hosts\n");
  append("10.0.0.1 other.local o1\n");
  append("::1 lo6.local lo6\n");
  append("10.9.8.7 app.local app a1\n");
  append("10.2.0.9 nul.local bad");
  db[pos++] = '\0';
  append("alias\n");

  // >600-byte /etc/hosts reverse line with 20 aliases.
  append("10.2.0.1 wide20.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char alias[16];
    make_rev_alias(alias, i);
    db[pos++] = ' ';
    append(string_view(alias, 12));
  }
  for (size_t i = 0; i < 40; ++i)
    append("          ");
  db[pos++] = '\n';

  ScopedTestDbFile hdb(libc_make_test_file_path("hba_hosts.test"),
                       string_view(db, pos),
                       &LIBC_NAMESPACE::netdb::set_hosts_path);

  // Embedded NUL before '\n' is rejected and never resolves.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr(NUL4, 4, AF_INET), nullptr);
  EXPECT_EQ(*LIBC_NAMESPACE::netdb::h_errno_addr(), HOST_NOT_FOUND);

  // Matching IPv4 record packs canonical name and aliases.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(APP4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "app.local");
  EXPECT_STREQ(he.h_aliases[0], "app");
  EXPECT_STREQ(he.h_aliases[1], "a1");
  EXPECT_EQ(he.h_aliases[2], nullptr);
  EXPECT_EQ(he.h_addrtype, AF_INET);
  EXPECT_EQ(he.h_length, 4);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[0]), uint8_t(10));
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[3]), uint8_t(7));
  EXPECT_EQ(he.h_addr_list[1], nullptr);

  // ERANGE with zero-length nullptr buffer and tight buffer sweep.
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(APP4, 4, AF_INET, &he, nullptr, 0,
                                            &res, &herr),
            ERANGE);
  EXPECT_EQ(res, nullptr);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  for (size_t cap = 1; cap < 80; ++cap) {
    herr = 0;
    const int RC = LIBC_NAMESPACE::gethostbyaddr_r(APP4, 4, AF_INET, &he, buf,
                                                   cap, &res, &herr);
    if (RC != 0) {
      EXPECT_EQ(RC, ERANGE);
      EXPECT_EQ(res, nullptr);
      EXPECT_EQ(herr, NETDB_INTERNAL);
    }
  }

  // >600B 20-alias reverse /etc/hosts record packs all 20 aliases in both _r
  // and thread-local gethostbyaddr.
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(WIDE4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "wide20.local");
  for (size_t i = 0; i < TWENTY_ALIASES; ++i) {
    char expected[16];
    make_rev_alias(expected, i);
    ASSERT_NE(he.h_aliases[i], nullptr);
    EXPECT_STREQ(he.h_aliases[i], expected);
  }
  EXPECT_EQ(he.h_aliases[TWENTY_ALIASES], nullptr);

  struct hostent *tls = LIBC_NAMESPACE::gethostbyaddr(WIDE4, 4, AF_INET);
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "wide20.local");
  EXPECT_STREQ(tls->h_aliases[19], "rev_alias_19");
  EXPECT_EQ(tls->h_aliases[20], nullptr);

  // V4-mapped IPv6 address matches IPv4 /etc/hosts entry while returning 16B
  // AF_INET6 hostent.
  uint8_t v6_mapped[16]{};
  v6_mapped[10] = 0xff;
  v6_mapped[11] = 0xff;
  LIBC_NAMESPACE::inline_memcpy(v6_mapped + 12, APP4, 4);
  ASSERT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(v6_mapped, 16, AF_INET6, &he, buf,
                                            sizeof(buf), &res, &herr),
            0);
  ASSERT_EQ(res, &he);
  EXPECT_STREQ(he.h_name, "app.local");
  EXPECT_STREQ(he.h_aliases[0], "app");
  EXPECT_EQ(he.h_addrtype, AF_INET6);
  EXPECT_EQ(he.h_length, 16);
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[10]), uint8_t(0xff));
  EXPECT_EQ(static_cast<uint8_t>(he.h_addr[15]), uint8_t(7));

  uint8_t v6_lo[16]{};
  v6_lo[15] = 1;
  tls = LIBC_NAMESPACE::gethostbyaddr(v6_lo, 16, AF_INET6);
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "lo6.local");
}

TEST_F(LlvmLibcGethostbyaddrTest, DnsPtrValidationAndIoErrors) {
  using namespace LIBC_NAMESPACE::netdb;
  using LIBC_NAMESPACE::cpp::span;

  ScopedTestDbFile hdb(libc_make_test_file_path("hba_ptr_hosts.test"), "",
                       &set_hosts_path);
  ScopedTestDbFile rdb(libc_make_test_file_path("hba_ptr_resolv.test"),
                       "nameserver 127.0.0.1\noptions attempts:1 timeout:1\n",
                       &set_resolv_conf_path);

  set_dns_wire_hook([](const ScopedIpAddr &, bool, span<const uint8_t> q,
                       span<uint8_t> r) -> int {
    const size_t Q = q.size();
    LIBC_NAMESPACE::inline_memcpy(r.data(), q.data(), Q);
    const char C = static_cast<char>(q[13]);
    if (C == '8') {
      write_u16_be(r, 2, DNS_FLAG_QR | DNS_RCODE_NXDOMAIN);
      return static_cast<int>(Q);
    }
    if (C == '2') {
      write_u16_be(r, 2, DNS_FLAG_QR | DNS_RCODE_SERVFAIL);
      return static_cast<int>(Q);
    }

    write_u16_be(r, 2, DNS_FLAG_QR);
    write_u16_be(r, 6, 1);
    if (C == '5') {
      // Root '.' PTR target rejected.
      const uint8_t ROOT_RR[13] = {
          0xc0, 12, 0, 12, 0, 1, 0, 0, 0, 5, 0, 1, 0,
      };
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, ROOT_RR, sizeof(ROOT_RR));
      return static_cast<int>(Q + sizeof(ROOT_RR));
    }
    if (C == '3') {
      // Non-RFC-952/1123 underscore PTR target rejected.
      const uint8_t BAD_RR[16] = {
          0xc0, 12, 0, 12, 0, 1, 0, 0, 0, 5, 0, 4, 2, 'a', '_', 0,
      };
      LIBC_NAMESPACE::inline_memcpy(r.data() + Q, BAD_RR, sizeof(BAD_RR));
      return static_cast<int>(Q + sizeof(BAD_RR));
    }

    const uint8_t OK_RR[20] = {
        0xc0, 12, 0, 12,  0,   1,   0, 0,   0,   5,
        0,    8,  3, 'p', 't', 'r', 2, 'i', 'o', 0,
    };
    LIBC_NAMESPACE::inline_memcpy(r.data() + Q, OK_RR, sizeof(OK_RR));
    return static_cast<int>(Q + sizeof(OK_RR));
  });

  static constexpr uint8_t OK4[4] = {192, 0, 2, 9};
  static constexpr uint8_t MISS4[4] = {192, 0, 2, 8};
  static constexpr uint8_t RETRY4[4] = {192, 0, 2, 2};
  static constexpr uint8_t ROOT4[4] = {192, 0, 2, 5};
  static constexpr uint8_t BAD4[4] = {192, 0, 2, 3};
  uint8_t ok6[16]{};
  ok6[0] = 0x20;
  ok6[15] = 9;

  struct hostent *tls = LIBC_NAMESPACE::gethostbyaddr(OK4, 4, AF_INET);
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "ptr.io");
  EXPECT_EQ(tls->h_addrtype, AF_INET);
  EXPECT_EQ(tls->h_length, 4);

  tls = LIBC_NAMESPACE::gethostbyaddr(ok6, 16, AF_INET6);
  ASSERT_NE(tls, nullptr);
  EXPECT_STREQ(tls->h_name, "ptr.io");
  EXPECT_EQ(tls->h_addrtype, AF_INET6);
  EXPECT_EQ(tls->h_length, 16);

  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr(MISS4, 4, AF_INET), nullptr);
  EXPECT_EQ(*h_errno_addr(), HOST_NOT_FOUND);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr(ROOT4, 4, AF_INET), nullptr);
  EXPECT_EQ(*h_errno_addr(), HOST_NOT_FOUND);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr(BAD4, 4, AF_INET), nullptr);
  EXPECT_EQ(*h_errno_addr(), HOST_NOT_FOUND);
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr(RETRY4, 4, AF_INET), nullptr);
  EXPECT_EQ(*h_errno_addr(), TRY_AGAIN);

  struct hostent he{};
  struct hostent *res = nullptr;
  alignas(char *) char buf[256];
  int herr = 0;

  // ERANGE on PTR hostent packing.
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(OK4, 4, AF_INET, &he, buf, 4, &res,
                                            &herr),
            ERANGE);
  EXPECT_EQ(herr, NETDB_INTERNAL);

  // resolv.conf and hosts ENOTDIR + EISDIR errors set NO_RECOVERY.
  char notdir_path[280];
  const size_t BASE_LEN = LIBC_NAMESPACE::internal::strlcpy(
      notdir_path, rdb.get_path(), sizeof(notdir_path));
  LIBC_NAMESPACE::internal::strlcpy(notdir_path + BASE_LEN, "/resolv.conf",
                                    sizeof(notdir_path) - BASE_LEN);
  set_resolv_conf_path(notdir_path);
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(OK4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            ENOTDIR);
  EXPECT_EQ(herr, NO_RECOVERY);

  set_hosts_path(notdir_path);
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(OK4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            ENOTDIR);
  EXPECT_EQ(herr, NO_RECOVERY);

  set_hosts_path(".");
  herr = 0;
  EXPECT_EQ(LIBC_NAMESPACE::gethostbyaddr_r(OK4, 4, AF_INET, &he, buf,
                                            sizeof(buf), &res, &herr),
            EISDIR);
  EXPECT_EQ(herr, NO_RECOVERY);
}

#ifdef LIBC_ADD_NULL_CHECKS
TEST_F(LlvmLibcGethostbyaddrTest, NullptrCrash) {
  struct hostent he{};
  struct hostent *r = nullptr;
  char b[8]{};
  EXPECT_DEATH([] { LIBC_NAMESPACE::gethostbyaddr(nullptr, 4, AF_INET); },
               WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyaddr_r(nullptr, 4, AF_INET, &he, b, sizeof(b),
                                        &r, nullptr);
      },
      WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyaddr_r("abcd", 4, AF_INET, nullptr, b,
                                        sizeof(b), &r, nullptr);
      },
      WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyaddr_r("abcd", 4, AF_INET, &he, nullptr,
                                        sizeof(b), &r, nullptr);
      },
      WITH_SIGNAL(-1));
  EXPECT_DEATH(
      [&] {
        LIBC_NAMESPACE::gethostbyaddr_r("abcd", 4, AF_INET, &he, b, sizeof(b),
                                        nullptr, nullptr);
      },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
