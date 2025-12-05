//===-- Macros defined in regex.h header file -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_REGEX_MACROS_H
#define LLVM_LIBC_MACROS_REGEX_MACROS_H

// Compilation flags (cflags) for regcomp()
#define REG_EXTENDED 1  // Use Extended Regular Expressions (ERE)
#define REG_ICASE 2     // Case-insensitive matching
#define REG_NOSUB 4     // Report only success/fail, not match positions
#define REG_NEWLINE 8   // Special newline handling

// Execution flags (eflags) for regexec()
#define REG_NOTBOL 1 // ^ does not match beginning of string
#define REG_NOTEOL 2 // $ does not match end of string

// Error codes returned by regcomp() and regexec()
#define REG_NOMATCH 1   // regexec() failed to match
#define REG_BADPAT 2    // Invalid regular expression
#define REG_ECOLLATE 3  // Invalid collating element referenced
#define REG_ECTYPE 4    // Invalid character class type referenced
#define REG_EESCAPE 5   // Trailing backslash in pattern
#define REG_ESUBREG 6   // Number in \digit invalid or in error
#define REG_EBRACK 7    // [] imbalance
#define REG_EPAREN 8    // \(\) or () imbalance
#define REG_EBRACE 9    // \{\} imbalance
#define REG_BADBR 10    // Content of \{\} invalid
#define REG_ERANGE 11   // Invalid endpoint in range expression
#define REG_ESPACE 12   // Out of memory
#define REG_BADRPT 13   // ?, *, or + not preceded by valid RE

// Implementation limits
// RE_DUP_MAX is the maximum number of times a BRE or ERE element
// can be repeated using the interval notation \{m,n\} or {m,n}.
// POSIX requires at least 255.
#define RE_DUP_MAX 255

#endif // LLVM_LIBC_MACROS_REGEX_MACROS_H
