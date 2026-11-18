//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for getaddrinfo.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/netdb_macros.h"
#include "hdr/netinet_in_macros.h"
#include "hdr/signal_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/in_port_t.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_addrinfo.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/net/byte_order.h"
#include "src/__support/netdb/addrinfo.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/getaddrinfo.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"
#include "test/src/netdb/test_db_file.h"

using namespace LIBC_NAMESPACE::testing::ErrnoSetterMatcher;

class LlvmLibcGetaddrinfoTest
    : public LIBC_NAMESPACE::testing::ErrnoCheckingTest {
protected:
  void TearDown() override {
    LIBC_NAMESPACE::netdb::clear_addrinfo_alloc_budget();
    LIBC_NAMESPACE::netdb::set_services_path(nullptr);
    ErrnoCheckingTest::TearDown();
  }
};

TEST_F(LlvmLibcGetaddrinfoTest, ValidationAndErrorCodes) {
  ScopedTestDbFile sdb(libc_make_test_file_path("getaddrinfo_serv.test"),
                       "http 80/tcp www\ntftp 69/udp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  struct addrinfo *raw = nullptr;

  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, nullptr, nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(raw, nullptr);

  struct addrinfo hints{};
  hints.ai_flags = AI_CANONNAME;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "80", &hints, &raw),
            EAI_BADFLAGS);

  struct HintCase {
    int flags;
    int family;
    int socktype;
    int protocol;
    int expected;
  };
  static constexpr HintCase ERROR_CASES[] = {
      {-1, AF_UNSPEC, 0, 0, EAI_BADFLAGS},
      {0, AF_UNIX, 0, 0, EAI_FAMILY},
      {0, AF_INET6, 0, 0, EAI_NONAME},
      {0, AF_UNSPEC, SOCK_STREAM, IPPROTO_UDP, EAI_SOCKTYPE},
      {0, AF_UNSPEC, SOCK_DGRAM, IPPROTO_TCP, EAI_SOCKTYPE},
      {0, AF_UNSPEC, SOCK_RAW, -1, EAI_SOCKTYPE},
      {0, AF_UNSPEC, SOCK_RAW, IPPROTO_ICMP, EAI_SERVICE},
  };
  for (const HintCase &C : ERROR_CASES) {
    hints = {};
    hints.ai_flags = C.flags;
    hints.ai_family = C.family;
    hints.ai_socktype = C.socktype;
    hints.ai_protocol = C.protocol;
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "80", &hints, &raw),
              C.expected);
    EXPECT_EQ(raw, nullptr);
  }

  static constexpr const char *BAD_SERVICES[] = {
      "", "/", "70000", "80x", "unknown",
  };
  for (const char *serv_str : BAD_SERVICES) {
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", serv_str, nullptr, &raw),
              EAI_SERVICE);
  }

  hints = {};
  hints.ai_flags = AI_NUMERICSERV;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "http", &hints, &raw),
            EAI_NONAME);

  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("host.invalid", "80", nullptr, &raw),
            EAI_NONAME);
}

TEST_F(LlvmLibcGetaddrinfoTest, Ipv4LiteralsServicesPassiveCanonAndOom) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::netdb::AddrinfoPtr;

  ScopedTestDbFile sdb(libc_make_test_file_path("getaddrinfo_serv_v4.test"),
                       "http 80/tcp www\ntftp 69/udp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  struct addrinfo *raw = nullptr;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("192.0.2.9", "www", nullptr, &raw), 0);
  AddrinfoPtr res(raw);
  ASSERT_NE(res.get(), nullptr);
  EXPECT_EQ(res->ai_family, AF_INET);
  EXPECT_EQ(res->ai_socktype, SOCK_STREAM);
  EXPECT_EQ(res->ai_protocol, IPPROTO_TCP);
  EXPECT_EQ(res->ai_next, nullptr);

  const auto *sa = reinterpret_cast<const struct sockaddr_in *>(res->ai_addr);
  ASSERT_NE(sa, nullptr);
  EXPECT_EQ(static_cast<int>(sa->sin_family), AF_INET);
  EXPECT_EQ(sa->sin_port, hton<in_port_t>(80));
  EXPECT_EQ(sa->sin_addr.s_addr, hton<uint32_t>(0xc0000209U));

  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "tftp", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_DGRAM);
  EXPECT_EQ(
      reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)->sin_port,
      hton<in_port_t>(69));

  hints = {};
  hints.ai_flags = AI_CANONNAME | AI_NUMERICHOST;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("192.0.2.1", "443", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_STREQ(res->ai_canonname, "192.0.2.1");
  ASSERT_NE(res->ai_next, nullptr);
  EXPECT_EQ(res->ai_next->ai_canonname, nullptr);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_STREAM);
  EXPECT_EQ(res->ai_next, nullptr);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_protocol = IPPROTO_TCP;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_STREAM);
  EXPECT_EQ(res->ai_next, nullptr);

  hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_RAW;
  hints.ai_protocol = IPPROTO_ICMP;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_RAW);
  EXPECT_EQ(res->ai_protocol, IPPROTO_ICMP);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", nullptr, nullptr, &raw),
            0);
  res.reset(raw);
  ASSERT_NE(res->ai_next, nullptr);
  ASSERT_NE(res->ai_next->ai_next, nullptr);
  EXPECT_EQ(res->ai_next->ai_next->ai_socktype, SOCK_RAW);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "80", nullptr, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_family, AF_INET);
  EXPECT_EQ(reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)
                ->sin_addr.s_addr,
            hton<uint32_t>(0x7f000001U));

  hints = {};
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(reinterpret_cast<const struct sockaddr_in *>(res->ai_addr)
                ->sin_addr.s_addr,
            uint32_t(0));

  for (size_t budget = 0; budget < 2; ++budget) {
    LIBC_NAMESPACE::netdb::set_addrinfo_alloc_budget(budget);
    EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "80", nullptr, &raw),
              EAI_MEMORY);
    EXPECT_EQ(raw, nullptr);
  }
}

TEST_F(LlvmLibcGetaddrinfoTest, Ipv6LiteralsScopeV4MappedAndServiceErrors) {
  using LIBC_NAMESPACE::net::hton;
  using LIBC_NAMESPACE::netdb::AddrinfoPtr;

  ScopedTestDbFile sdb(libc_make_test_file_path("getaddrinfo_serv_v6.test"),
                       "http 80/tcp\n",
                       &LIBC_NAMESPACE::netdb::set_services_path);

  struct addrinfo *raw = nullptr;
  struct addrinfo hints{};
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo(nullptr, "http", &hints, &raw), 0);
  AddrinfoPtr res(raw);
  ASSERT_NE(res.get(), nullptr);
  const auto *s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(static_cast<int>(s6->sin6_family), AF_INET6);
  EXPECT_EQ(s6->sin6_port, hton<in_port_t>(80));
  EXPECT_EQ(s6->sin6_addr.s6_addr[15], uint8_t(0));

  hints = {};
  hints.ai_family = AF_INET;
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("::1", "80", &hints, &raw), EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%eth0", "80", nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%", "80", nullptr, &raw),
            EAI_NONAME);
  EXPECT_EQ(
      LIBC_NAMESPACE::getaddrinfo("fe80::1%4294967296", "80", nullptr, &raw),
      EAI_NONAME);

  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("fe80::1%2", "80", nullptr, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(s6->sin6_scope_id, uint32_t(2));
  EXPECT_EQ(s6->sin6_addr.s6_addr[0], uint8_t(0xfe));

  ASSERT_EQ(
      LIBC_NAMESPACE::getaddrinfo("::ffff:192.0.2.1", "80", nullptr, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(s6->sin6_addr.s6_addr[10], uint8_t(0xff));
  EXPECT_EQ(s6->sin6_addr.s6_addr[15], uint8_t(1));

  hints = {};
  hints.ai_flags = AI_V4MAPPED;
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("192.0.2.1", "80", &hints, &raw), 0);
  res.reset(raw);
  s6 = reinterpret_cast<const struct sockaddr_in6 *>(res->ai_addr);
  EXPECT_EQ(s6->sin6_addr.s6_addr[10], uint8_t(0xff));
  EXPECT_EQ(s6->sin6_addr.s6_addr[11], uint8_t(0xff));
  EXPECT_EQ(s6->sin6_addr.s6_addr[15], uint8_t(1));

  hints = {};
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_DGRAM;
  ASSERT_EQ(LIBC_NAMESPACE::getaddrinfo("::1", "80", &hints, &raw), 0);
  res.reset(raw);
  EXPECT_EQ(res->ai_socktype, SOCK_DGRAM);

  LIBC_NAMESPACE::netdb::set_services_path("/dev/null/not_a_dir");
  hints = {};
  hints.ai_family = AF_INET6;
  hints.ai_socktype = SOCK_STREAM;
  EXPECT_THAT(LIBC_NAMESPACE::getaddrinfo("::1", "http", &hints, &raw),
              Fails<int>(ENOTDIR, EAI_SYSTEM));
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcGetaddrinfoTest, NullResCrash) {
  ASSERT_DEATH(
      [] { LIBC_NAMESPACE::getaddrinfo("127.0.0.1", "80", nullptr, nullptr); },
      WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
