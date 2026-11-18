//===-- Unittests for bsearch ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/stdlib/bsearch.h"

#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/span.h"
#include "test/UnitTest/Test.h"

#include "hdr/types/size_t.h"

static int int_compare(const void *l, const void *r) {
  int li = *reinterpret_cast<const int *>(l);
  int ri = *reinterpret_cast<const int *>(r);
  if (li == ri)
    return 0;
  else if (li > ri)
    return 1;
  else
    return -1;
}

TEST(LlvmLibcBsearchTest, ErrorInputs) {
  int val = 123;
  EXPECT_TRUE(LIBC_NAMESPACE::bsearch(nullptr, &val, 1, sizeof(int),
                                      int_compare) == nullptr);
  EXPECT_TRUE(LIBC_NAMESPACE::bsearch(&val, nullptr, 1, sizeof(int),
                                      int_compare) == nullptr);
  EXPECT_TRUE(LIBC_NAMESPACE::bsearch(&val, &val, 0, sizeof(int),
                                      int_compare) == nullptr);
  EXPECT_TRUE(LIBC_NAMESPACE::bsearch(&val, &val, 1, 0, int_compare) ==
              nullptr);
  EXPECT_TRUE(LIBC_NAMESPACE::bsearch(
                  &val, &val,
                  LIBC_NAMESPACE::cpp::numeric_limits<size_t>::max(),
                  sizeof(int), int_compare) == nullptr);
}

TEST(LlvmLibcBsearchTest, IntegerArray) {
  constexpr int ARRAY[25] = {10,   23,   33,    35,   55,   70,   71,
                             100,  110,  123,   133,  135,  155,  170,
                             171,  1100, 1110,  1123, 1133, 1135, 1155,
                             1170, 1171, 11100, 12310};
  const LIBC_NAMESPACE::cpp::span<const int> array_span(ARRAY);

  for (size_t s = 1; s <= array_span.size(); ++s) {
    auto prefix = array_span.first(s);
    for (int key : prefix) {
      void *elem = LIBC_NAMESPACE::bsearch(&key, prefix.data(), prefix.size(),
                                           sizeof(int), int_compare);
      ASSERT_EQ(*reinterpret_cast<int *>(elem), key);
    }
  }

  // Non existent keys
  constexpr int NON_EXISTENT[] = {5, 125, 136, 12345};
  for (size_t s = 1; s <= array_span.size(); ++s) {
    auto prefix = array_span.first(s);
    for (int key : NON_EXISTENT) {
      ASSERT_TRUE(LIBC_NAMESPACE::bsearch(&key, prefix.data(), prefix.size(),
                                          sizeof(int), int_compare) == nullptr);
    }
  }
}

TEST(LlvmLibcBsearchTest, SameKeyAndArray) {
  constexpr int ARRAY[5] = {1, 2, 3, 4, 5};
  const LIBC_NAMESPACE::cpp::span<const int> array_span(ARRAY);
  void *elem =
      LIBC_NAMESPACE::bsearch(array_span.data(), array_span.data(),
                              array_span.size(), sizeof(int), int_compare);
  EXPECT_EQ(*reinterpret_cast<int *>(elem), array_span.front());
}
