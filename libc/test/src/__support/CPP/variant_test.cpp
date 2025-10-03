//===-- Unittests for Variant --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "src/__support/CPP/variant.h"
#include "test/UnitTest/Test.h"

using LIBC_NAMESPACE::cpp::variant;
using LIBC_NAMESPACE::cpp::get;

// Simple POD types for testing
struct SimpleStruct {
  int value;
  
  SimpleStruct() : value(0) {}
  SimpleStruct(int v) : value(v) {}
  
  bool operator==(const SimpleStruct& other) const {
    return value == other.value;
  }
};

TEST(LlvmLibcVariantTest, TwoTypeVariant) {
  // Test default construction
  variant<int, double> v1;
  ASSERT_EQ(v1.index(), 0);
  ASSERT_EQ(get<int>(v1), 0);

  // Test construction from first type
  variant<int, double> v2(42);
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 42);

  // Test construction from second type
  variant<int, double> v3(3.14);
  ASSERT_EQ(v3.index(), 1);
  ASSERT_EQ(get<double>(v3), 3.14);

  // Test copy construction
  variant<int, double> v4(v2);
  ASSERT_EQ(v4.index(), 0);
  ASSERT_EQ(get<int>(v4), 42);

  // Test assignment
  v1 = v3;
  ASSERT_EQ(v1.index(), 1);
  ASSERT_EQ(get<double>(v1), 3.14);

  // Test member get
  int& int_ref = v2.get<int>();
  ASSERT_EQ(int_ref, 42);
  int_ref = 99;
  ASSERT_EQ(get<int>(v2), 99);
}

TEST(LlvmLibcVariantTest, ThreeTypeVariant) {
  // Test default construction
  variant<int, double, SimpleStruct> v1;
  ASSERT_EQ(v1.index(), 0);
  ASSERT_EQ(get<int>(v1), 0);

  // Test construction from first type
  variant<int, double, SimpleStruct> v2(42);
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 42);

  // Test construction from second type
  variant<int, double, SimpleStruct> v3(3.14);
  ASSERT_EQ(v3.index(), 1);
  ASSERT_EQ(get<double>(v3), 3.14);

  // Test construction from third type
  SimpleStruct s(123);
  variant<int, double, SimpleStruct> v4(s);
  ASSERT_EQ(v4.index(), 2);
  ASSERT_EQ(get<SimpleStruct>(v4).value, 123);

  // Test copy construction
  variant<int, double, SimpleStruct> v5(v4);
  ASSERT_EQ(v5.index(), 2);
  ASSERT_EQ(get<SimpleStruct>(v5).value, 123);

  // Test assignment
  v1 = v3;
  ASSERT_EQ(v1.index(), 1);
  ASSERT_EQ(get<double>(v1), 3.14);

  // Test modifying through reference
  SimpleStruct& struct_ref = v4.get<SimpleStruct>();
  struct_ref.value = 456;
  ASSERT_EQ(get<SimpleStruct>(v4).value, 456);
}

TEST(LlvmLibcVariantTest, MoveSemantics) {
  // Test move construction
  variant<int, double> v1(42);
  variant<int, double> v2(LIBC_NAMESPACE::cpp::move(v1));
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 42);

  // Test move assignment
  variant<int, double> v3(3.14);
  v2 = LIBC_NAMESPACE::cpp::move(v3);
  ASSERT_EQ(v2.index(), 1);
  ASSERT_EQ(get<double>(v2), 3.14);
}

TEST(LlvmLibcVariantTest, ConstAccess) {
  const variant<int, double> v(42);
  ASSERT_EQ(v.index(), 0);
  ASSERT_EQ(get<int>(v), 42);
  
  const int& const_ref = v.get<int>();
  ASSERT_EQ(const_ref, 42);
}