//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Pool for Regular Expression AST nodes (Class Definitions).
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_EXPR_POOL_H
#define LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_EXPR_POOL_H

#include "src/__support/CPP/expected.h"
#include "src/__support/macros/config.h"
#include "src/__support/regex/regex_ast.h"
#include <stddef.h>

namespace LIBC_NAMESPACE_DECL {
namespace regex {

/// An arena-based pool for Regular Expression AST nodes.
///
/// This class manages the allocation and hash-consing of Expr nodes. All
/// nodes created through this pool are owned by it and will be freed when
/// the pool is destroyed. Hash-consing ensures that identical expressions
/// are represented by the same pointer, enabling fast comparison and
/// derivative normalization.
class ExprPool {
  /// Internal storage block for AST nodes.
  ///
  /// Blocks are allocated on demand to avoid large contiguous allocations.
  struct Block {
    /// Number of Expr nodes stored in each block.
    static constexpr size_t BLOCK_SIZE = 256;
    /// The actual storage for Expr nodes.
    Expr nodes[BLOCK_SIZE];
  };

  static constexpr size_t BLOCK_SIZE = Block::BLOCK_SIZE;
  static constexpr size_t MAX_NODE_LIMIT = 10000;
  static constexpr size_t MAX_BLOCKS =
      (MAX_NODE_LIMIT + BLOCK_SIZE - 1) / BLOCK_SIZE; // 40

  /// Array of block pointers for O(1) index resolution.
  Block *blocks[MAX_BLOCKS];
  size_t block_count = 0;
  size_t node_count = 0;

  /// The size of the hash table used for hash-consing (interning) expression
  /// nodes. Choosing 0x4000 (16,384) is the smallest power of two that keeps
  /// the load factor below 70% when the pool reaches its limit of 10,000 nodes
  /// (peak load factor is ~61%). Using a power of two allows the compiler to
  /// optimize the modulo indexing into an efficient bitwise AND, while the low
  /// load factor minimizes collisions and guarantees O(1) average interning
  /// time.
  static constexpr size_t HASH_TABLE_SIZE = 0x4000;

  /// Hash table storing ExprIds of unique Expr nodes.
  /// 0 (INVALID_EXPR_ID) represents an empty bucket.
  ExprId *hashtable = nullptr;

  /// Core hash-consing function (Interning).
  ///
  /// Guarantees that for any two identical structural definitions of an Expr,
  /// this function will return the same ExprId. This enables O(1) structural
  /// equality via ID comparison.
  ///
  /// \param e A structural definition (proto-node) to intern.
  /// \returns The ExprId of the unique, stable instance in the arena,
  ///          or REG_ESPACE on failure.
  cpp::expected<ExprId, int> intern(const Expr &e);

public:
  ExprPool();
  ~ExprPool();

  /// Resolves an ExprId to a reference.
  const Expr &get(ExprId id) const {
    // Index is 1-based, so subtract 1 for internal storage.
    uint32_t internal_idx = id - 1;
    uint32_t block_idx = internal_idx / BLOCK_SIZE;
    uint32_t node_idx = internal_idx % BLOCK_SIZE;
    return blocks[block_idx]->nodes[node_idx];
  }

  Expr &get(ExprId id) {
    uint32_t internal_idx = id - 1;
    uint32_t block_idx = internal_idx / BLOCK_SIZE;
    uint32_t node_idx = internal_idx % BLOCK_SIZE;
    return blocks[block_idx]->nodes[node_idx];
  }

  // TODO: Use fluent interface (and_then, transform) for these factories once
  // implemented in cpp::expected.

  /// Returns an EmptySet node.
  cpp::expected<ExprId, int> empty_set();
  /// Returns an EmptyStr node.
  cpp::expected<ExprId, int> empty_str();
  /// Creates or returns an existing Literal node for the given character.
  cpp::expected<ExprId, int> make_lit(char c);
  /// Normalizing factory for Concatenation (L · R).
  ///
  /// Applies algebraic simplifications before interning:
  /// - (Ø · R) or (R · Ø) => Ø
  /// - (ε · R) or (R · ε) => R
  cpp::expected<ExprId, int> make_concat(ExprId l, ExprId r);

  /// Normalizing factory for Alternation (L | R).
  ///
  /// Applies algebraic simplifications before interning:
  /// - (Ø | R) or (R | Ø) => R
  /// - (R | R) => R (Idempotency)
  cpp::expected<ExprId, int> make_alt(ExprId l, ExprId r);
};

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_REGEX_REGEX_EXPR_POOL_H
