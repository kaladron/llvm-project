//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Fuzz target for DNS wire encoding, pointer decompression, response
/// validation, PTR QNAME synthesis, and UDP/TCP resolver state machines.
///
//===----------------------------------------------------------------------===//

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "hdr/types/ssize_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/span.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/network.h"
#include "src/__support/ctype_utils.h"
#include "src/__support/netdb/dns_client.h"
#include "src/__support/netdb/dns_ptr.h"
#include "src/__support/netdb/dns_response.h"
#include "src/__support/netdb/dns_transport.h"
#include "src/__support/netdb/dns_wire.h"
#include "src/__support/netdb/hosts.h"
#include "src/__support/netdb/ip_parse.h"
#include "src/__support/netdb/resolv_conf.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace {

using namespace LIBC_NAMESPACE::netdb;
using LIBC_NAMESPACE::inline_memcpy;
using LIBC_NAMESPACE::cpp::array;
using LIBC_NAMESPACE::cpp::optional;
using LIBC_NAMESPACE::cpp::span;
using LIBC_NAMESPACE::cpp::string_view;
using LIBC_NAMESPACE::network::UdpRecvKind;
using LIBC_NAMESPACE::network::UdpRecvResult;

void trap_if(bool cond) {
  if (cond)
    __builtin_trap();
}

struct FuzzDnsIo {
  span<const uint8_t> rem{};
  uint64_t now = 100;
  uint16_t last_txid = 0x4242;
  int open_balance = 0;

  uint8_t pull_u8() {
    if (rem.empty())
      return 0;
    const uint8_t BYTE = rem[0];
    rem = rem.subspan(1);
    return BYTE;
  }

  uint64_t now_ms() const { return now; }

  uint16_t random_txid() {
    const uint16_t HI = static_cast<uint16_t>(pull_u8()) << 8;
    const uint16_t LO = pull_u8();
    last_txid = static_cast<uint16_t>(HI | LO);
    return last_txid;
  }

  int open_udp(const ScopedIpAddr &) {
    if ((pull_u8() & 7u) == 0)
      return -1;
    ++open_balance;
    return 10;
  }

  int open_tcp(const ScopedIpAddr &, uint32_t) {
    if ((pull_u8() & 7u) == 0)
      return -1;
    ++open_balance;
    return 11;
  }

  void close_fd(int fd) {
    trap_if(fd < 0 || open_balance <= 0);
    --open_balance;
  }

  ssize_t udp_send(int, span<const uint8_t> pkt) {
    trap_if(pkt.size() < DNS_HEADER_LEN + 5);
    if ((pull_u8() & 7u) == 0)
      return -1;
    return static_cast<ssize_t>(pkt.size());
  }

  UdpRecvResult udp_poll_recv(int, span<uint8_t> out, uint32_t) {
    now += static_cast<uint64_t>(pull_u8()) * 10ULL + 1ULL;
    const uint8_t ACT = pull_u8() & 3u;
    if (ACT == 0)
      return {UdpRecvKind::TIMEOUT, 0};
    if (ACT == 1)
      return {UdpRecvKind::HARD_ERROR, 0};
    if (ACT == 2)
      return {UdpRecvKind::TRUNCATED, out.size() + 1};

    size_t n = pull_u8();
    if (n > rem.size())
      n = rem.size();
    if (n > out.size())
      n = out.size();
    inline_memcpy(out.data(), rem.data(), n);
    rem = rem.subspan(n);

    if (n >= 2 && (pull_u8() & 1u) != 0)
      write_u16_be(out, 0, last_txid);
    return {UdpRecvKind::PACKET, n};
  }

  ssize_t tcp_send(int, span<const uint8_t> buf, uint32_t) {
    now += static_cast<uint64_t>(pull_u8()) + 1ULL;
    if ((pull_u8() & 7u) == 0)
      return -1;
    const size_t SENT = (pull_u8() % buf.size()) + 1;
    return static_cast<ssize_t>(SENT);
  }

  ssize_t tcp_recv(int, span<uint8_t> buf, uint32_t) {
    now += static_cast<uint64_t>(pull_u8()) + 1ULL;
    const uint8_t TAG = pull_u8();
    if ((TAG & 7u) == 0)
      return -1;
    if (rem.empty() || (TAG & 7u) == 1)
      return 0;

    size_t n = (TAG % 32u) + 1;
    if (n > buf.size())
      n = buf.size();
    if (n > rem.size())
      n = rem.size();
    inline_memcpy(buf.data(), rem.data(), n);
    rem = rem.subspan(n);
    return static_cast<ssize_t>(n);
  }
};

struct FuzzDispatchCtx {
  FuzzDnsIo io;
  bool use_vc = false;
};

DnsStatus fuzz_ns_query(void *raw_ctx, const ScopedIpAddr &ns,
                        span<const uint8_t> qname, uint16_t qtype,
                        uint32_t timeout_ms, DnsParsedResponse &out) {
  auto &ctx = *static_cast<FuzzDispatchCtx *>(raw_ctx);
  if (!ctx.use_vc) {
    const DnsStatus ST =
        run_udp_query(ctx.io, ns, qname, qtype, timeout_ms, out);
    if (ST != DnsStatus::TRUNCATED)
      return ST;
  }
  return run_tcp_query(ctx.io, ns, qname, qtype, timeout_ms, out);
}

void check_validated_wire(span<const uint8_t> wire) {
  array<char, DNS_MAX_TEXT_NAME + 1> text{};
  const size_t TLEN = wire_to_presentation(wire, text);
  trap_if(TLEN > DNS_MAX_TEXT_NAME || text[TLEN] != '\0');
  for (size_t i = 0; i < TLEN; ++i) {
    const uint8_t BYTE = static_cast<uint8_t>(text[i]);
    trap_if(BYTE <= 0x20 || BYTE == 0x7f);
  }
}

void fuzz_qname_and_ptr(uint8_t sub, span<const uint8_t> payload) {
  const string_view INPUT(reinterpret_cast<const char *>(payload.data()),
                          payload.size());
  (void)classify_special_domain(INPUT);
  (void)is_valid_ptr_hostname(INPUT);

  array<uint8_t, DNS_MAX_WIRE_NAME> wire{};
  array<uint8_t, DNS_MAX_WIRE_NAME> dec_wire{};
  const optional<EncodedQname> ENC = encode_qname(INPUT, wire);
  if (ENC.has_value()) {
    array<uint8_t, DNS_HEADER_LEN + DNS_MAX_WIRE_NAME> pkt{};
    inline_memcpy(pkt.data() + DNS_HEADER_LEN, wire.data(), ENC->wire_len);
    const size_t END = DNS_HEADER_LEN + ENC->wire_len;
    const optional<DecodedWireName> DEC =
        read_wire_name({pkt.data(), END}, DNS_HEADER_LEN, END, dec_wire);
    trap_if(!DEC.has_value() || DEC->consumed != ENC->wire_len);
    trap_if(!wire_names_equal({dec_wire.data(), DEC->wire_len},
                              {wire.data(), ENC->wire_len}));

    array<char, DNS_MAX_TEXT_NAME + 1> pres{};
    const size_t PLEN =
        wire_to_presentation({dec_wire.data(), DEC->wire_len}, pres);
    const string_view EXPECTED =
        ENC->is_fqdn ? INPUT.substr(0, INPUT.size() - 1) : INPUT;
    trap_if(string_view(pres.data(), PLEN) != EXPECTED);
  }

  if (payload.size() > DNS_HEADER_LEN) {
    const size_t START =
        DNS_HEADER_LEN + (sub % (payload.size() - DNS_HEADER_LEN));
    const optional<DecodedWireName> DEC =
        read_wire_name(payload, START, payload.size(), dec_wire);
    if (DEC.has_value())
      check_validated_wire({dec_wire.data(), DEC->wire_len});
  }

  if (payload.size() >= 16) {
    array<uint8_t, IPV6_PTR_WIRE_LEN> p4{};
    array<uint8_t, IPV6_PTR_WIRE_LEN> p6{};
    const size_t L4 = build_ipv4_ptr_qname(payload.subspan(0, 4), p4);
    const size_t L6 = build_ipv6_ptr_qname(payload.subspan(0, 16), p6);
    check_validated_wire({p4.data(), L4});
    check_validated_wire({p6.data(), L6});

    const size_t SPLIT = sub % payload.size();
    (void)apply_ni_nofqdn(INPUT.substr(0, SPLIT), INPUT.substr(SPLIT));
  }
}

void fuzz_response(uint8_t sub, span<const uint8_t> payload) {
  static constexpr uint8_t DEFAULT_QNAME[3] = {1, 'a', 0};
  array<uint8_t, DNS_MAX_WIRE_NAME> dyn{};
  span<const uint8_t> qname(DEFAULT_QNAME, sizeof(DEFAULT_QNAME));

  if ((sub & 1u) != 0 && payload.size() > 4) {
    const size_t NAME_LEN = payload[0] % 32u;
    if (1 + NAME_LEN < payload.size()) {
      const string_view CAND(
          reinterpret_cast<const char *>(payload.subspan(1).data()), NAME_LEN);
      const optional<EncodedQname> ENC = encode_qname(CAND, dyn);
      if (ENC.has_value()) {
        qname = {dyn.data(), ENC->wire_len};
        payload = payload.subspan(1 + NAME_LEN);
      }
    }
  }

  static constexpr uint16_t QTYPES[3] = {
      DNS_TYPE_A,
      DNS_TYPE_AAAA,
      DNS_TYPE_PTR,
  };
  const uint16_t QTYPE = QTYPES[(sub >> 1) % 3u];
  const uint16_t TXID =
      payload.size() >= 2 ? read_u16_be(payload, 0) : uint16_t(0x1234);

  DnsParsedResponse out{};
  if (parse_dns_response(payload, TXID, qname, QTYPE, out) == DnsStatus::OK)
    check_validated_wire({out.canon_wire.data(), out.clen});
}

void populate_fuzz_config(uint8_t sub, string_view conf, ResolvConfig &cfg) {
  size_t pos = 0;
  while (pos < conf.size()) {
    while (pos < conf.size() && LIBC_NAMESPACE::internal::isspace(conf[pos]))
      ++pos;
    const size_t START = pos;
    while (pos < conf.size() && !LIBC_NAMESPACE::internal::isspace(conf[pos]))
      ++pos;
    const string_view TOK = conf.substr(START, pos - START);
    if (!TOK.empty()) {
      ScopedIpAddr ns{};
      if (cfg.num_nameservers < RESOLV_MAX_NS && parse_scoped_ip(TOK, ns)) {
        cfg.nameservers[cfg.num_nameservers++] = ns;
      } else {
        apply_resolv_option(TOK, cfg);
        append_search_domain(TOK, cfg);
      }
    }
  }

  if (cfg.num_nameservers == 0) {
    cfg.nameservers[0].addr[0] = 127;
    cfg.nameservers[0].addr[3] = 1;
    cfg.num_nameservers = 1;
  }
  cfg.timeout = 1;
  cfg.attempts = static_cast<uint8_t>((sub % 2u) + 1u);
  cfg.use_vc = (sub & 4u) != 0;
}

void fuzz_transport(uint8_t sub, span<const uint8_t> payload) {
  const size_t CONF_LEN = payload.size() / 3;
  const size_t NAME_LEN = (payload.size() - CONF_LEN) / 4;
  const string_view CONF_TEXT(reinterpret_cast<const char *>(payload.data()),
                              CONF_LEN);
  const string_view QUERY_TEXT(
      reinterpret_cast<const char *>(payload.subspan(CONF_LEN).data()),
      NAME_LEN);

  ResolvConfig cfg{};
  populate_fuzz_config(sub, CONF_TEXT, cfg);

  FuzzDispatchCtx ctx{FuzzDnsIo{payload.subspan(CONF_LEN + NAME_LEN)},
                      cfg.use_vc};
  const size_t START_NS = sub % cfg.num_nameservers;
  const DnsResolver RESOLVER{&fuzz_ns_query, &ctx, cfg, START_NS};

  DnsParsedResponse out{};
  static constexpr uint8_t PTR_QNAME[3] = {1, 'a', 0};
  const bool USE_RAW_PTR =
      (sub & 2u) != 0 ||
      classify_special_domain(QUERY_TEXT) == SpecialDomainKind::INVALID;

  DnsStatus st = DnsStatus::NXDOMAIN;
  if (USE_RAW_PTR) {
    st = RESOLVER.query_qname(PTR_QNAME, DNS_TYPE_PTR, out);
  } else {
    const uint16_t QTYPE = (sub & 1u) != 0 ? DNS_TYPE_A : DNS_TYPE_AAAA;
    st = RESOLVER.resolve_name(QUERY_TEXT, QTYPE, out);
  }

  trap_if(ctx.io.open_balance != 0);
  trap_if(st == DnsStatus::IGNORE || st == DnsStatus::TRUNCATED);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 3)
    return 0;

  const uint8_t MODE = data[0] % 3u;
  const uint8_t SUB = data[1];
  const span<const uint8_t> PAYLOAD =
      span<const uint8_t>(data, size).subspan(2);

  if (MODE == 0)
    fuzz_qname_and_ptr(SUB, PAYLOAD);
  else if (MODE == 1)
    fuzz_response(SUB, PAYLOAD);
  else
    fuzz_transport(SUB, PAYLOAD);

  return 0;
}
