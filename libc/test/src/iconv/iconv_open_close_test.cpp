//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Unittests for iconv_open and iconv_close.
///
//===----------------------------------------------------------------------===//

#include "hdr/errno_macros.h"
#include "hdr/types/iconv_t.h"
#include "src/__support/CPP/scope.h"
#include "src/iconv/iconv_close.h"
#include "src/iconv/iconv_open.h"
#include "test/UnitTest/ErrnoCheckingTest.h"
#include "test/UnitTest/ErrnoSetterMatcher.h"
#include "test/UnitTest/Test.h"

using LlvmLibcIconvOpenCloseTest = LIBC_NAMESPACE::testing::ErrnoCheckingTest;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::EQ;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Fails;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::NE;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::returns;
using LIBC_NAMESPACE::testing::ErrnoSetterMatcher::Succeeds;

static iconv_t invalid_iconv() { return reinterpret_cast<iconv_t>(-1); }

static void *as_ptr(iconv_t cd) { return static_cast<void *>(cd); }

#define EXPECT_OPEN_CLOSE(tocode, fromcode)                                    \
  do {                                                                         \
    iconv_t cd = nullptr;                                                      \
    ASSERT_THAT(as_ptr(cd = LIBC_NAMESPACE::iconv_open((tocode), (fromcode))), \
                returns(NE(as_ptr(invalid_iconv()))).with_errno(EQ(0)));       \
    ASSERT_NE(cd, nullptr);                                                    \
    EXPECT_THAT(LIBC_NAMESPACE::iconv_close(cd), Succeeds(0));                 \
  } while (0)

TEST_F(LlvmLibcIconvOpenCloseTest, ValidEncodingPairs) {
  EXPECT_OPEN_CLOSE("US-ASCII", "ISO-8859-1");
  EXPECT_OPEN_CLOSE("ISO-8859-1", "US-ASCII");
  EXPECT_OPEN_CLOSE("ASCII", "ASCII");
  EXPECT_OPEN_CLOSE("ISO-8859-1", "ISO-8859-1");

  EXPECT_OPEN_CLOSE("us-ascii", "US_ASCII");
  EXPECT_OPEN_CLOSE("usascii", "ascii");
  EXPECT_OPEN_CLOSE("ANSI_X3.4-1968", "ansix3.41968");
  EXPECT_OPEN_CLOSE("ISO646-US", "iso_646_us");
  EXPECT_OPEN_CLOSE("646", "ASCII");

  EXPECT_OPEN_CLOSE("ISO_8859-1", "iso88591");
  EXPECT_OPEN_CLOSE("LATIN1", "latin-1");
  EXPECT_OPEN_CLOSE("LATIN_1", "ISO-8859-1");

  iconv_t cd1 = nullptr;
  ASSERT_THAT(as_ptr(cd1 = LIBC_NAMESPACE::iconv_open("ASCII", "ISO-8859-1")),
              returns(NE(as_ptr(invalid_iconv()))).with_errno(EQ(0)));
  ASSERT_NE(cd1, nullptr);
  LIBC_NAMESPACE::cpp::scope_exit guard1(
      [&] { EXPECT_THAT(LIBC_NAMESPACE::iconv_close(cd1), Succeeds(0)); });

  iconv_t cd2 = nullptr;
  ASSERT_THAT(as_ptr(cd2 = LIBC_NAMESPACE::iconv_open("ASCII", "ISO-8859-1")),
              returns(NE(as_ptr(invalid_iconv()))).with_errno(EQ(0)));
  ASSERT_NE(cd2, nullptr);
  LIBC_NAMESPACE::cpp::scope_exit guard2(
      [&] { EXPECT_THAT(LIBC_NAMESPACE::iconv_close(cd2), Succeeds(0)); });
  EXPECT_NE(cd1, cd2);
}

TEST_F(LlvmLibcIconvOpenCloseTest, InvalidEncodingsReturnEinval) {
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("UTF-8", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ASCII", "UTF-8")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));

  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ASCIIX", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ASC", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("64", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ASCII", "")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("---", "___")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));

  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ASCII//TRANSLIT", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ASCII//IGNORE", "ASCII")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("ISO-8859-2", "ISO-8859-1")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
  EXPECT_THAT(as_ptr(LIBC_NAMESPACE::iconv_open("UTF-16", "UTF-32")),
              Fails<void *>(EINVAL, as_ptr(invalid_iconv())));
}

TEST_F(LlvmLibcIconvOpenCloseTest, InvalidDescriptorsInIconvClose) {
  EXPECT_THAT(LIBC_NAMESPACE::iconv_close(nullptr), Fails(EBADF));
  EXPECT_THAT(LIBC_NAMESPACE::iconv_close(invalid_iconv()), Fails(EBADF));
}

#if defined(LIBC_ADD_NULL_CHECKS)
TEST_F(LlvmLibcIconvOpenCloseTest, NullPointerCrash) {
  ASSERT_DEATH([] { LIBC_NAMESPACE::iconv_open(nullptr, "ASCII"); },
               WITH_SIGNAL(-1));
  ASSERT_DEATH([] { LIBC_NAMESPACE::iconv_open("ASCII", nullptr); },
               WITH_SIGNAL(-1));
}
#endif // LIBC_ADD_NULL_CHECKS
