//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Fuzz target for LineReader, netdb flat-file databases, and all 23
/// <netdb.h> entrypoints.
///
//===----------------------------------------------------------------------===//

#include "hdr/netdb_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/sys_socket_macros.h"
#include "hdr/types/size_t.h"
#include "hdr/types/socklen_t.h"
#include "hdr/types/struct_addrinfo.h"
#include "hdr/types/struct_hostent.h"
#include "hdr/types/struct_netent.h"
#include "hdr/types/struct_protoent.h"
#include "hdr/types/struct_servent.h"
#include "hdr/types/struct_sockaddr.h"
#include "hdr/types/struct_sockaddr_in.h"
#include "hdr/types/struct_sockaddr_in6.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/scope.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/stringstream.h"
#include "src/__support/File/file.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/line_reader.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/__support/netdb/services.h"
#include "src/netdb/__h_errno_location.h"
#include "src/netdb/freeaddrinfo.h"
#include "src/netdb/gai_strerror.h"
#include "src/netdb/getaddrinfo.h"
#include "src/netdb/gethostbyaddr.h"
#include "src/netdb/gethostbyaddr_r.h"
#include "src/netdb/gethostbyname.h"
#include "src/netdb/gethostbyname2.h"
#include "src/netdb/gethostbyname2_r.h"
#include "src/netdb/gethostbyname_r.h"
#include "src/netdb/getnameinfo.h"
#include "src/netdb/getnet_internal.h"
#include "src/netdb/getnetbyaddr.h"
#include "src/netdb/getnetbyaddr_r.h"
#include "src/netdb/getnetbyname.h"
#include "src/netdb/getnetbyname_r.h"
#include "src/netdb/getproto_internal.h"
#include "src/netdb/getprotobyname.h"
#include "src/netdb/getprotobyname_r.h"
#include "src/netdb/getprotobynumber.h"
#include "src/netdb/getprotobynumber_r.h"
#include "src/netdb/getservbyname.h"
#include "src/netdb/getservbyname_r.h"
#include "src/netdb/getservbyport.h"
#include "src/netdb/getservbyport_r.h"
#include "src/stdio/remove.h"
#include "src/string/memory_utils/inline_memcpy.h"
#include "src/string/memory_utils/inline_memset.h"

namespace {

using namespace LIBC_NAMESPACE::netdb;
using LIBC_NAMESPACE::inline_memcpy;
using LIBC_NAMESPACE::inline_memset;
using LIBC_NAMESPACE::cpp::array;
using LIBC_NAMESPACE::cpp::scope_exit;
using LIBC_NAMESPACE::cpp::span;
using LIBC_NAMESPACE::cpp::string_view;
using LIBC_NAMESPACE::cpp::StringStream;

constexpr char GUARD_BYTE = 0x5a;
LIBC_THREAD_LOCAL span<const uint8_t> g_dns_fuzz{};

struct ScopedFuzzDb {
  const char *path;
  void (*setter)(const char *);

  ScopedFuzzDb(const char *p, span<const uint8_t> bytes,
               void (*fn)(const char *))
      : path(p), setter(fn) {
    auto file_or = LIBC_NAMESPACE::openfile(path, "w");
    if (file_or.has_value()) {
      LIBC_NAMESPACE::File *file = file_or.value();
      (void)file->write(bytes.data(), bytes.size());
      (void)file->close();
    }
    setter(path);
  }

  ~ScopedFuzzDb() {
    setter(nullptr);
    (void)LIBC_NAMESPACE::remove(path);
  }
};

void check_guard(const array<char, 256> &buf, size_t cap) {
  for (size_t i = cap; i < buf.size(); ++i) {
    if (buf[i] != GUARD_BYTE)
      __builtin_trap();
  }
}

int fuzz_dns_hook(const ScopedIpAddr &, bool, span<const uint8_t> q,
                  span<uint8_t> r) {
  if (g_dns_fuzz.empty() || (g_dns_fuzz[0] & 3u) == 0)
    return -1;

  const size_t N = g_dns_fuzz.size() < r.size() ? g_dns_fuzz.size() : r.size();
  inline_memcpy(r.data(), g_dns_fuzz.data(), N);
  if (N >= 2 && q.size() >= 2 && (g_dns_fuzz[0] & 4u) != 0) {
    r[0] = q[0];
    r[1] = q[1];
  }
  return static_cast<int>(N);
}

void make_cstr(span<const uint8_t> src, array<char, 32> &dst) {
  const size_t N = src.size() < 31 ? src.size() : 31;
  for (size_t i = 0; i < N; ++i)
    dst[i] = src[i] == 0 ? 'a' : static_cast<char>(src[i]);
  dst[N] = '\0';
}

void exercise_line_reader(const char *path, string_view target, uint8_t sub) {
  auto file_or = LIBC_NAMESPACE::openfile(path, "re");
  if (!file_or.has_value())
    return;

  LIBC_NAMESPACE::File *file = file_or.value();
  scope_exit close_file([file]() { file->close(); });
  LIBC_NAMESPACE::File::FileLock lock(file);
  LineReader reader(file, (sub & 1u) != 0 ? ';' : '#');

  while (true) {
    reader.begin_line();
    size_t count = 0;
    while (true) {
      char tok_buf[64];
      StringStream tok_out(tok_buf);
      auto tok_or = reader.read_token(tok_out, target, (sub & 2u) != 0);
      if (!tok_or.has_value() || !tok_or->has_value())
        break;
      ++count;
    }
    if (count == 0 && !reader.has_bytes())
      break;
    (void)reader.is_invalid();
  }
}

void fuzz_proto_net_serv(uint8_t mode, uint8_t sub,
                         span<const uint8_t> payload) {
  array<char, 32> name{};
  array<char, 32> proto_buf{};
  const size_t NAME_LEN = static_cast<size_t>(sub & 7u) < payload.size()
                              ? (sub & 7u)
                              : payload.size();
  make_cstr(payload.subspan(0, NAME_LEN), name);
  make_cstr(payload.subspan(payload.size() / 2), proto_buf);
  const char *PROTO = (sub & 8u) != 0 ? nullptr : proto_buf.data();

  alignas(char *) array<char, 256> buf{};
  inline_memset(buf.data(), GUARD_BYTE, buf.size());
  const size_t CAP = sub % 200u;

  if (mode == 0) {
    ScopedFuzzDb db("fuzz_proto.db", payload, &set_protocols_path);
    exercise_line_reader("fuzz_proto.db", name.data(), sub);

    struct protoent pe{};
    struct protoent *res = nullptr;
    (void)LIBC_NAMESPACE::getprotobyname(name.data());
    (void)LIBC_NAMESPACE::getprotobynumber(static_cast<int>(sub));
    (void)LIBC_NAMESPACE::getprotobyname_r(name.data(), &pe, buf.data(), CAP,
                                           &res);
    check_guard(buf, CAP);

    inline_memset(buf.data(), GUARD_BYTE, buf.size());
    (void)LIBC_NAMESPACE::getprotobynumber_r(static_cast<int>(sub), &pe,
                                             buf.data(), CAP, &res);
    check_guard(buf, CAP);
    return;
  }

  if (mode == 1) {
    ScopedFuzzDb db("fuzz_net.db", payload, &set_networks_path);
    struct netent ne{};
    struct netent *res = nullptr;
    int herr = 0;
    const int AF = (sub & 16u) != 0 ? AF_INET : AF_INET6;

    (void)LIBC_NAMESPACE::getnetbyname(name.data());
    (void)LIBC_NAMESPACE::getnetbyaddr(static_cast<uint32_t>(sub), AF);
    (void)LIBC_NAMESPACE::getnetbyname_r(name.data(), &ne, buf.data(), CAP,
                                         &res, &herr);
    check_guard(buf, CAP);

    inline_memset(buf.data(), GUARD_BYTE, buf.size());
    (void)LIBC_NAMESPACE::getnetbyaddr_r(static_cast<uint32_t>(sub), AF, &ne,
                                         buf.data(), CAP, &res, &herr);
    check_guard(buf, CAP);
    return;
  }

  ScopedFuzzDb db("fuzz_serv.db", payload, &set_services_path);
  struct servent se{};
  struct servent *res = nullptr;
  (void)LIBC_NAMESPACE::getservbyname(name.data(), PROTO);
  (void)LIBC_NAMESPACE::getservbyport(static_cast<int>(sub), PROTO);
  (void)LIBC_NAMESPACE::getservbyname_r(name.data(), PROTO, &se, buf.data(),
                                        CAP, &res);
  check_guard(buf, CAP);

  inline_memset(buf.data(), GUARD_BYTE, buf.size());
  (void)LIBC_NAMESPACE::getservbyport_r(static_cast<int>(sub), PROTO, &se,
                                        buf.data(), CAP, &res);
  check_guard(buf, CAP);
}

void fuzz_hosts_and_gai(uint8_t mode, uint8_t sub,
                        span<const uint8_t> payload) {
  const size_t HLEN = payload.size() / 3;
  const size_t RLEN = (payload.size() - HLEN) / 3;
  g_dns_fuzz = payload.subspan(HLEN + RLEN);

  ScopedFuzzDb hdb("fuzz_hosts.db", payload.subspan(0, HLEN), &set_hosts_path);
  ScopedFuzzDb rdb("fuzz_resolv.db", payload.subspan(HLEN, RLEN),
                   &set_resolv_conf_path);
  scope_exit reset_hooks([]() {
    set_dns_wire_hook(nullptr);
    LIBC_NAMESPACE::network::clear_addrconfig_override();
  });
  set_dns_wire_hook(&fuzz_dns_hook);
  LIBC_NAMESPACE::network::set_addrconfig_override(sub & 3u);

  array<char, 32> name{};
  array<char, 32> serv{};
  make_cstr(g_dns_fuzz, name);
  make_cstr(payload, serv);
  const int AF = (sub & 1u) != 0 ? AF_INET6 : AF_INET;
  const size_t CAP = sub % 200u;

  if (mode == 3) {
    alignas(char *) array<char, 256> buf{};
    struct hostent he{};
    struct hostent *res = nullptr;
    int herr = 0;

    (void)LIBC_NAMESPACE::gethostbyname(name.data());
    (void)LIBC_NAMESPACE::gethostbyname2(name.data(), AF);

    inline_memset(buf.data(), GUARD_BYTE, buf.size());
    (void)LIBC_NAMESPACE::gethostbyname_r(name.data(), &he, buf.data(), CAP,
                                          &res, &herr);
    check_guard(buf, CAP);

    inline_memset(buf.data(), GUARD_BYTE, buf.size());
    (void)LIBC_NAMESPACE::gethostbyname2_r(name.data(), AF, &he, buf.data(),
                                           CAP, &res, &herr);
    check_guard(buf, CAP);

    array<uint8_t, 16> raw{};
    for (size_t i = 0; i < raw.size() && i < payload.size(); ++i)
      raw[i] = payload[i];
    const socklen_t ADDR_LEN = AF == AF_INET6 ? 16 : 4;
    (void)LIBC_NAMESPACE::gethostbyaddr(raw.data(), ADDR_LEN, AF);

    inline_memset(buf.data(), GUARD_BYTE, buf.size());
    (void)LIBC_NAMESPACE::gethostbyaddr_r(raw.data(), ADDR_LEN, AF, &he,
                                          buf.data(), CAP, &res, &herr);
    check_guard(buf, CAP);

    (void)*LIBC_NAMESPACE::__h_errno_location();
    return;
  }

  ScopedFuzzDb sdb("fuzz_gserv.db", payload.subspan(0, HLEN),
                   &set_services_path);
  struct addrinfo hints{};
  struct addrinfo *ai = nullptr;
  hints.ai_flags = static_cast<int>(sub);
  hints.ai_family = (sub & 2u) != 0 ? AF_UNSPEC : AF;

  const char *NODE_ARG = (sub & 4u) != 0 ? nullptr : name.data();
  const char *SERV_ARG = (sub & 8u) != 0 ? nullptr : serv.data();
  const struct addrinfo *HINTS_ARG = (sub & 16u) != 0 ? nullptr : &hints;
  const int GAI_RC =
      LIBC_NAMESPACE::getaddrinfo(NODE_ARG, SERV_ARG, HINTS_ARG, &ai);
  if (LIBC_NAMESPACE::gai_strerror(GAI_RC) == nullptr)
    __builtin_trap();
  LIBC_NAMESPACE::freeaddrinfo(ai);

  struct sockaddr_in6 s6{};
  struct sockaddr_in s4{};
  s6.sin6_family = AF_INET6;
  s4.sin_family = AF_INET;
  for (size_t i = 0; i < 16 && i < payload.size(); ++i)
    s6.sin6_addr.s6_addr[i] = payload[i];
  inline_memcpy(&s4.sin_addr, s6.sin6_addr.s6_addr, sizeof(s4.sin_addr));

  array<char, 256> node_buf{};
  array<char, 256> serv_buf{};
  inline_memset(node_buf.data(), GUARD_BYTE, node_buf.size());
  inline_memset(serv_buf.data(), GUARD_BYTE, serv_buf.size());
  const size_t SCAP = (sub >> 2) % 64u;

  const struct sockaddr *SA =
      AF == AF_INET6 ? reinterpret_cast<const struct sockaddr *>(&s6)
                     : reinterpret_cast<const struct sockaddr *>(&s4);
  const socklen_t SALEN = AF == AF_INET6 ? static_cast<socklen_t>(sizeof(s6))
                                         : static_cast<socklen_t>(sizeof(s4));
  const int NI_FLAGS = static_cast<int>(sub & 0x1fu) |
                       ((sub & 0x20u) != 0 ? NI_NUMERICSCOPE : 0);
  (void)LIBC_NAMESPACE::getnameinfo(
      SA, SALEN, node_buf.data(), static_cast<socklen_t>(CAP), serv_buf.data(),
      static_cast<socklen_t>(SCAP), NI_FLAGS);
  check_guard(node_buf, CAP);
  check_guard(serv_buf, SCAP);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 3)
    return 0;

  const uint8_t MODE = data[0] % 5u;
  const uint8_t SUB = data[1];
  const span<const uint8_t> PAYLOAD =
      span<const uint8_t>(data, size).subspan(2);

  if (MODE < 3)
    fuzz_proto_net_serv(MODE, SUB, PAYLOAD);
  else
    fuzz_hosts_and_gai(MODE, SUB, PAYLOAD);

  return 0;
}
