//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unit tests for freeaddrinfo.
///
//===----------------------------------------------------------------------===//

#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "hdr/types/struct_addrinfo.h"
#include "hdr/types/struct_sockaddr.h"
#include "src/__support/CPP/new.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/netdb/addrinfo.h"
#include "src/netdb/freeaddrinfo.h"
#include "test/UnitTest/Test.h"

namespace {

struct addrinfo *make_test_node(size_t extra) {
  auto raw = LIBC_NAMESPACE::make_unique<uint8_t[]>(
      sizeof(LIBC_NAMESPACE::netdb::AddrinfoNode) + extra);
  if (!raw)
    return nullptr;

  uint8_t *buf = raw.release();
  auto *node = new (buf) LIBC_NAMESPACE::netdb::AddrinfoNode{};
  node->ai.ai_addr = reinterpret_cast<struct sockaddr *>(&node->sa);
  return &node->ai;
}

} // namespace

TEST(LlvmLibcFreeaddrinfoTest, NullChainAndDetachedSublist) {
  LIBC_NAMESPACE::freeaddrinfo(nullptr);

  struct addrinfo *n1 = make_test_node(8);
  struct addrinfo *n2 = make_test_node(0);
  struct addrinfo *n3 = make_test_node(0);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);
  ASSERT_NE(n3, nullptr);
  n1->ai_next = n2;
  n2->ai_next = n3;

  struct addrinfo *sublist = n1->ai_next;
  n1->ai_next = nullptr;
  LIBC_NAMESPACE::freeaddrinfo(sublist);
  LIBC_NAMESPACE::freeaddrinfo(n1);
}
