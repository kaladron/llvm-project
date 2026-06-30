//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Pool for Regular Expression AST nodes (Implementation).
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_expr_pool.h"
#include "hdr/regex_macros.h"
#include "src/__support/CPP/new.h"
#include "src/__support/alloc-checker.h"
#include "src/__support/hash.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/null_check.h"
#include "src/string/memory_utils/inline_memset.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Hash an Expr node for hash-consing.
uint64_t hash_expr(const Expr &e) {
  // Initialise HashState with a constant seed. The specific value (0x12345678)
  // is an arbitrary placeholder; HashState immediately mixes this seed with
  // high-entropy constants (derived from aHash) to produce a strong hash, while
  // the constant value guarantees deterministic hashing for hash-consing.
  internal::HashState hasher(0x12345678);
  uint64_t kind = static_cast<uint64_t>(e.kind);
  hasher.update(&kind, sizeof(kind));
  switch (e.kind) {
  case ExprKind::Literal:
    hasher.update(&e.ch, sizeof(e.ch));
    break;
  case ExprKind::Concat:
  case ExprKind::Alt:
    hasher.update(&e.bin.left, sizeof(e.bin.left));
    hasher.update(&e.bin.right, sizeof(e.bin.right));
    break;
  default:
    break;
  }
  return hasher.finish();
}

} // namespace

ExprPool::ExprPool() {
  AllocChecker ac;
  hashtable = new (ac) ExprId[HASH_TABLE_SIZE];
  if (ac) {
    inline_memset(hashtable, 0, HASH_TABLE_SIZE * sizeof(ExprId));
  }
  inline_memset(blocks, 0, MAX_BLOCKS * sizeof(Block *));
}

ExprPool::~ExprPool() {
  if (hashtable)
    delete[] hashtable;
  for (size_t i = 0; i < block_count; ++i) {
    delete blocks[i];
  }
}

cpp::expected<ExprId, int> ExprPool::intern(const Expr &e) {
  if (!hashtable)
    return cpp::unexpected(REG_ESPACE);

  // 1. Calculate the initial bucket for the given structural definition.
  uint64_t h = hash_expr(e);
  size_t idx = h % HASH_TABLE_SIZE;

  // 2. Linear Probing: Search for an existing node with identical content.
  //    Empty bucket is represented by 0 (INVALID_EXPR_ID).
  size_t start_idx = idx;
  while (hashtable[idx] != INVALID_EXPR_ID) {
    if (get(hashtable[idx]) == e)
      return hashtable[idx];
    idx = (idx + 1) % HASH_TABLE_SIZE;
    if (idx == start_idx) {
      // Table full
      return cpp::unexpected(REG_ESPACE);
    }
  }

  // 3. Admission Control: Check the hard limit on AST nodes.
  if (node_count >= MAX_NODE_LIMIT)
    return cpp::unexpected(REG_ESPACE);

  // 4. Arena Allocation:
  uint32_t internal_idx = static_cast<uint32_t>(node_count);
  uint32_t block_idx = internal_idx / BLOCK_SIZE;
  uint32_t node_idx = internal_idx % BLOCK_SIZE;

  if (node_idx == 0) {
    if (block_idx >= MAX_BLOCKS)
      return cpp::unexpected(REG_ESPACE);
    AllocChecker ac;
    blocks[block_idx] = new (ac) Block();
    if (!ac)
      return cpp::unexpected(REG_ESPACE);
    block_count++;
  }

  // 5. Node Initialisation: Copy the structural definition into the arena.
  blocks[block_idx]->nodes[node_idx] = e;

  // 1-based index for ID.
  ExprId new_id = internal_idx + 1;
  hashtable[idx] = new_id;
  node_count++;
  return new_id;
}

cpp::expected<ExprId, int> ExprPool::empty_set() {
  return intern(Expr::make_empty_set());
}
cpp::expected<ExprId, int> ExprPool::empty_str() {
  return intern(Expr::make_empty_str());
}
cpp::expected<ExprId, int> ExprPool::make_lit(char c) {
  return intern(Expr::make_literal(c));
}

cpp::expected<ExprId, int> ExprPool::make_concat(ExprId l, ExprId r) {
  if (l == INVALID_EXPR_ID || r == INVALID_EXPR_ID)
    return cpp::unexpected(REG_BADPAT);

  const Expr &left_node = get(l);
  const Expr &right_node = get(r);

  // Apply basic algebraic identities for concatenation:
  // 1. Ø · R = R · Ø = Ø (Identity: null set)
  if (left_node.kind == ExprKind::EmptySet ||
      right_node.kind == ExprKind::EmptySet)
    return empty_set();
  // 2. ε · R = R · ε = R (Identity: empty string)
  if (left_node.kind == ExprKind::EmptyStr)
    return r;
  if (right_node.kind == ExprKind::EmptyStr)
    return l;
  return intern(Expr::make_concat(l, r));
}

cpp::expected<ExprId, int> ExprPool::make_alt(ExprId l, ExprId r) {
  if (l == INVALID_EXPR_ID || r == INVALID_EXPR_ID)
    return cpp::unexpected(REG_BADPAT);

  const Expr &left_node = get(l);
  const Expr &right_node = get(r);

  // Apply basic algebraic identities for alternation:
  // 1. Ø | R = R | Ø = R (Identity: null set)
  if (left_node.kind == ExprKind::EmptySet)
    return r;
  if (right_node.kind == ExprKind::EmptySet)
    return l;
  // 2. R | R = R (Idempotency)
  if (l == r)
    return l;
  return intern(Expr::make_alt(l, r));
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
