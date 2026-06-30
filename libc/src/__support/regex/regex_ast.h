//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// AST nodes for Regular Expressions (Class Definitions).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H

#include "hdr/stdint_proxy.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

using ExprId = uint32_t;
constexpr ExprId INVALID_EXPR_ID = 0;

/// Enumeration of Regular Expression AST node types.
enum class ExprKind {
  /// Represents the empty set (matches nothing).
  EmptySet,
  /// Represents the empty string (matches the empty string).
  EmptyStr,
  /// A literal character match.
  Literal,
  /// Concatenation of two expressions (left followed by right).
  Concat,
  /// Alternation between two expressions (left or right).
  Alt,
};

/// A node in the Regular Expression Abstract Syntax Tree.
///
/// Expressions are represented as a hash-consed DAG to enable efficient
/// derivative-based matching. This structure is intended to be managed by
/// an ExprPool.
///
/// DESIGN NOTE: Tagged Union vs. Polymorphism
/// This structure uses a tagged union (ExprKind + anonymous union) rather than
/// C++ polymorphism (subclasses with virtual functions) for several reasons:
/// 1. Memory Efficiency: Avoiding virtual functions saves the overhead of a
///    vtable pointer (8 bytes on 64-bit) per node. This is critical for keeping
///    leaf nodes small and improving cache locality during dense DAG traversal.
/// 2. Fixed-Size Allocation: A single fixed-size type allows the ExprPool arena
///    allocator to remain simple and fast, avoiding fragmentation or the need
///    for heterogeneous size management.
/// 3. Performance: Switch-on-kind avoids dynamic dispatch overhead in hot loops
///    (e.g., recursive derivative computation) and simplifies equality checks
///    required for hash-consing.
struct Expr {
  /// The type of this expression node.
  ExprKind kind;
  union {
    /// Character value for Literal nodes.
    char ch;
    /// Sub-expressions for Concat and Alt nodes.
    struct {
      ExprId left;
      ExprId right;
    } bin;
  };

  /// Default constructor creates an EmptySet node.
  /// Public to allow array allocation in ExprPool::Block.
  constexpr Expr() : kind(ExprKind::EmptySet), ch('\0') {}

private:
  /// Create a node of a specific kind with no data.
  constexpr Expr(ExprKind k) : kind(k), ch('\0') {}
  /// Create a Literal node.
  constexpr Expr(char c) : kind(ExprKind::Literal), ch(c) {}
  /// Create a binary node (Concat or Alt).
  constexpr Expr(ExprKind k, ExprId l, ExprId r) : kind(k), bin{l, r} {}

public:
  static constexpr Expr make_empty_set() { return Expr(ExprKind::EmptySet); }
  static constexpr Expr make_empty_str() { return Expr(ExprKind::EmptyStr); }
  static constexpr Expr make_literal(char c) { return Expr(c); }
  static constexpr Expr make_concat(ExprId l, ExprId r) {
    return Expr(ExprKind::Concat, l, r);
  }
  static constexpr Expr make_alt(ExprId l, ExprId r) {
    return Expr(ExprKind::Alt, l, r);
  }

  /// Equivalence check for hash-consing.
  bool operator==(const Expr &other) const {
    if (kind != other.kind)
      return false;
    switch (kind) {
    case ExprKind::EmptySet:
    case ExprKind::EmptyStr:
      return true;
    case ExprKind::Literal:
      return ch == other.ch;
    case ExprKind::Concat:
    case ExprKind::Alt:
      return bin.left == other.bin.left && bin.right == other.bin.right;
    }
    return false;
  }
};

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_AST_H
