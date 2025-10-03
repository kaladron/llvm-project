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

// POD types similar to what we use in time zone parsing
struct NonLeapDay {
  short day; // day of non-leap year [1:365]
  
  NonLeapDay() : day(0) {}
  NonLeapDay(short d) : day(d) {}
  
  bool operator==(const NonLeapDay& other) const {
    return day == other.day;
  }
};

struct Day {
  short day; // day of year [0:365]
  
  Day() : day(0) {}
  Day(short d) : day(d) {}
  
  bool operator==(const Day& other) const {
    return day == other.day;
  }
};

struct MonthWeekWeekday {
  char month;   // month of year [1:12]
  char week;    // week of month [1:5] (5==last)
  char weekday; // 0==Sun, ..., 6=Sat
  
  MonthWeekWeekday() : month(0), week(0), weekday(0) {}
  MonthWeekWeekday(char m, char w, char wd) : month(m), week(w), weekday(wd) {}
  
  bool operator==(const MonthWeekWeekday& other) const {
    return month == other.month && week == other.week && weekday == other.weekday;
  }
};

TEST(LlvmLibcVariantTest, TwoTypeVariant) {
  // Test default construction
  variant<int, char> v1;
  ASSERT_EQ(v1.index(), 0);
  ASSERT_EQ(get<int>(v1), 0);

  // Test construction from first type
  variant<int, char> v2(42);
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 42);

  // Test construction from second type
  variant<int, char> v3('z');
  ASSERT_EQ(v3.index(), 1);
  ASSERT_EQ(get<char>(v3), 'z');

  // Test copy construction
  variant<int, char> v4(v2);
  ASSERT_EQ(v4.index(), 0);
  ASSERT_EQ(get<int>(v4), 42);

  // Test assignment
  v1 = v3;
  ASSERT_EQ(v1.index(), 1);
  ASSERT_EQ(get<char>(v1), 'z');

  // Test member get
  int& int_ref = v2.get<int>();
  ASSERT_EQ(int_ref, 42);
  int_ref = 99;
  ASSERT_EQ(get<int>(v2), 99);
}

TEST(LlvmLibcVariantTest, ThreeTypeVariant) {
  // Test default construction
  variant<int, char, long> v1;
  ASSERT_EQ(v1.index(), 0);
  ASSERT_EQ(get<int>(v1), 0);

  // Test construction from first type
  variant<int, char, long> v2(42);
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 42);

  // Test construction from second type
  variant<int, char, long> v3('x');
  ASSERT_EQ(v3.index(), 1);
  ASSERT_EQ(get<char>(v3), 'x');

  // Test construction from third type
  variant<int, char, long> v4(123L);
  ASSERT_EQ(v4.index(), 2);
  ASSERT_EQ(get<long>(v4), 123L);

  // Test copy construction
  variant<int, char, long> v5(v4);
  ASSERT_EQ(v5.index(), 2);
  ASSERT_EQ(get<long>(v5), 123L);

  // Test assignment
  v1 = v3;
  ASSERT_EQ(v1.index(), 1);
  ASSERT_EQ(get<char>(v1), 'x');

  // Test modifying through reference
  long& long_ref = v4.get<long>();
  long_ref = 456L;
  ASSERT_EQ(get<long>(v4), 456L);
}

TEST(LlvmLibcVariantTest, MoveSemantics) {
  // Test move construction
  variant<int, char> v1(42);
  variant<int, char> v2(LIBC_NAMESPACE::cpp::move(v1));
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 42);

  // Test move assignment
  variant<int, char> v3('z');
  v2 = LIBC_NAMESPACE::cpp::move(v3);
  ASSERT_EQ(v2.index(), 1);
  ASSERT_EQ(get<char>(v2), 'z');
}

TEST(LlvmLibcVariantTest, ConstAccess) {
  const variant<int, char> v(42);
  ASSERT_EQ(v.index(), 0);
  ASSERT_EQ(get<int>(v), 42);
  
  const int& const_ref = v.get<int>();
  ASSERT_EQ(const_ref, 42);
}

// Test real-world scenario like TimeZone Date structures
TEST(LlvmLibcVariantTest, TimeZoneDateStructures) {
  // Test variant with simplified types
  variant<int, char> simplified_variant;
  
  // Test default construction (should be first type)
  ASSERT_EQ(simplified_variant.index(), 0);
  ASSERT_EQ(get<int>(simplified_variant), 0);
  
  // Test int assignment
  simplified_variant = 59;
  ASSERT_EQ(simplified_variant.index(), 0);
  ASSERT_EQ(get<int>(simplified_variant), 59);
  
  // Test char assignment  
  simplified_variant = 'A';
  ASSERT_EQ(simplified_variant.index(), 1);
  ASSERT_EQ(get<char>(simplified_variant), 'A');
}

// Test assignment compatibility (the issue we had with optional)
TEST(LlvmLibcVariantTest, AssignmentCompatibility) {
  variant<int, char> v1(42);
  variant<int, char> v2('x');
  
  // This type of assignment should work
  v1 = v2;
  ASSERT_EQ(v1.index(), 1);
  ASSERT_EQ(get<char>(v1), 'x');
  
  // Test multiple reassignments
  v2 = variant<int, char>(99);
  ASSERT_EQ(v2.index(), 0);
  ASSERT_EQ(get<int>(v2), 99);
  
  v1 = v2;
  ASSERT_EQ(v1.index(), 0);
  ASSERT_EQ(get<int>(v1), 99);
}

// Test POD constraint validation
TEST(LlvmLibcVariantTest, PODTypes) {
  // Test that our variant works with simple POD types
  variant<int, char> int_char_variant(100);
  
  ASSERT_EQ(get<int>(int_char_variant), 100);
  
  // Test assignment between different instances
  int_char_variant = 'A';
  ASSERT_EQ(get<char>(int_char_variant), 'A');
}

// Test type safety - this demonstrates the benefit over unsafe unions
TEST(LlvmLibcVariantTest, TypeSafety) {
  variant<int, char> v(42);
  
  // We can safely check which type is active
  ASSERT_EQ(v.index(), 0); // First type (int)
  
  // We can safely access the int value
  ASSERT_EQ(get<int>(v), 42);
  
  // Change to char type
  v = 'Z';
  ASSERT_EQ(v.index(), 1); // Second type (char) 
  ASSERT_EQ(get<char>(v), 'Z');
  
  // Index correctly tracks the active type
  // This type safety prevents the memory corruption issues we had with unions
}
