//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Brzozowski derivative evaluation and regular expression matching
/// implementation.
///
//===----------------------------------------------------------------------===//

#include "src/__support/regex/regex_matcher.h"

#include "hdr/regex_macros.h"
#include "hdr/stdint_proxy.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/algorithm.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/CPP/unique_ptr.h"
#include "src/__support/checked_ptr.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_assert.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"
#include "src/__support/regex/regex_ast.h"
#include "src/__support/regex/regex_expr_pool.h"

namespace LIBC_NAMESPACE_DECL {
namespace regex {

namespace {

// Computes the active AnchorKind bitmask at fencepost position pos in input.
uint8_t ctx_at(cpp::string_view input, size_t pos, int cflags, int eflags) {
  LIBC_ASSERT(pos <= input.size());
  uint8_t ctx = 0;
  if ((pos == 0 && !(eflags & REG_NOTBOL)) ||
      (pos > 0 && (cflags & REG_NEWLINE) && input[pos - 1] == '\n'))
    ctx |= ANCHOR_BOL;
  if ((pos == input.size() && !(eflags & REG_NOTEOL)) ||
      (pos < input.size() && (cflags & REG_NEWLINE) && input[pos] == '\n'))
    ctx |= ANCHOR_EOL;
  return ctx;
}

// Swaps ANCHOR_BOL and ANCHOR_EOL bits when reversing direction.
constexpr uint8_t swap_anchors(uint8_t mask) {
  return static_cast<uint8_t>(((mask & ANCHOR_BOL) ? ANCHOR_EOL : 0) |
                              ((mask & ANCHOR_EOL) ? ANCHOR_BOL : 0));
}

// Computes the active AnchorKind bitmask in reversed coordinates at pos.
uint8_t rev_ctx_at(cpp::string_view input, size_t pos, int cflags, int eflags) {
  return swap_anchors(ctx_at(input, pos, cflags, eflags));
}

constexpr ByteSet make_all_bytes_set() {
  ByteSet bs;
  bs.flip();
  return bs;
}
constexpr ByteSet ALL_BYTES_SET = make_all_bytes_set();

// Returns true iff id is Star(ByteSet) matching all 256 byte values.  Under
// REG_NEWLINE, '.' omits '\n', so this returns false and Pass 3 steps normally.
bool is_universal_star(ExprId id, const ExprPool &pool) {
  const Expr e = pool.get(id);
  if (e.kind != ExprKind::Star)
    return false;
  const Expr body = pool.get(e.body);
  return body.kind == ExprKind::ByteSet &&
         pool.get_byte_set(body.byte_set_id) == ALL_BYTES_SET;
}

// One parent node whose derivative is still being computed.
// d_left is INVALID_EXPR_ID while the node's first child (left or body) is
// being walked.  When a Concat or Alt finishes its left child and must walk its
// right child next, d_left saves the left-hand term until the right child's
// result arrives and the two are joined with make_alt.
// The default constructor is trivial so make_unique_for_overwrite does not zero
// the heap array.
struct DerivFrame {
  ExprId id;
  ExprId d_left;

  DerivFrame() = default;
  constexpr explicit DerivFrame(ExprId expr_id)
      : id(expr_id), d_left(INVALID_EXPR_ID) {
    LIBC_ASSERT(expr_id != INVALID_EXPR_ID);
  }
};
static_assert(sizeof(DerivFrame) == 8);
static_assert(cpp::is_trivially_constructible<DerivFrame>::value);

// One pending parent node in reverse_expr().
// For Concat, id is the chain root memoised on completion, acc accumulates the
// reversed prefix left-to-right, and tail tracks the remaining suffix so an
// entire right-associated Concat chain reverses in one frame.
// For Alt, acc is INVALID_EXPR_ID while walking left and holds the reversed
// left branch while walking right.
// The default constructor is trivial so make_unique_for_overwrite does not zero
// the heap array.
struct RevFrame {
  ExprId id;
  ExprId acc;
  ExprId tail;

  RevFrame() = default;
  constexpr RevFrame(ExprId expr_id, ExprId initial_acc, ExprId initial_tail)
      : id(expr_id), acc(initial_acc), tail(initial_tail) {
    LIBC_ASSERT(expr_id != INVALID_EXPR_ID);
  }
};
static_assert(sizeof(RevFrame) == 12);
static_assert(cpp::is_trivially_constructible<RevFrame>::value);

// Stack of traversal frames for iterative AST walks.  The first InlineCap
// frames live inside the object; deeper walks move to a heap array that doubles
// in size.  BlockStore is not used because its pop_back() frees a block each
// time depth drops back across a block boundary, and a tree walk crosses that
// boundary repeatedly.  match() and match_span() use one DerivStack for every
// input byte, so a heap array allocated for one byte is reused for the rest.
template <typename Frame, size_t InlineCap> class FrameStack {
  static_assert(cpp::is_trivially_constructible<Frame>::value);
  static_assert(InlineCap > 0 && InlineCap < ExprPool::MAX_NODE_LIMIT);
  static constexpr size_t GROWTH_FACTOR = 2;
  static_assert(ExprPool::MAX_NODE_LIMIT <= (~size_t(0)) / GROWTH_FACTOR);

  Frame inline_buf[InlineCap];
  cpp::unique_ptr<Frame[]> heap;
  size_t len = 0;
  size_t cap = InlineCap;

  Frame *data() { return heap ? heap.get() : inline_buf; }

  // Moves the frames to a heap array twice the size.  Because every pushed
  // frame has a smaller ExprId than its parent, the stack never needs more than
  // ExprPool::MAX_NODE_LIMIT frames.
  [[nodiscard]] bool grow() {
    LIBC_ASSERT(len == cap && cap < ExprPool::MAX_NODE_LIMIT);
    const size_t new_cap =
        cpp::min(cap * GROWTH_FACTOR, ExprPool::MAX_NODE_LIMIT);
    auto next = make_unique_for_overwrite<Frame[]>(new_cap);
    if (!next)
      return false;
    const Frame *src = data();
    for (size_t i = 0; i < len; ++i)
      next[i] = src[i];
    heap = next.take_unique();
    cap = new_cap;
    return true;
  }

public:
  FrameStack() = default;
  // The implicit move operations would move heap while copying len and cap,
  // leaving the source stack in an inconsistent state.
  FrameStack(FrameStack &&) = delete;
  FrameStack &operator=(FrameStack &&) = delete;

  bool empty() const { return len == 0; }
  void clear() { len = 0; }

  Frame &top() {
    LIBC_ASSERT(len > 0);
    return data()[len - 1];
  }

  void pop() {
    LIBC_ASSERT(len > 0);
    --len;
  }

  [[nodiscard]] bool push(Frame frame) {
    if (LIBC_UNLIKELY(len == cap) && !grow())
      return false;
    LIBC_ASSERT(len < cap);
    data()[len] = frame;
    ++len;
    return true;
  }
};

// 8 frames (64 bytes) handle shallow expressions inline without heap growth.
using DerivStack = FrameStack<DerivFrame, 8>;
// Concat chains reuse a single frame, so 4 frames (48 bytes) suffice inline.
using RevStack = FrameStack<RevFrame, 4>;

// Computes D_c(root) iteratively in post-order.  stack is passed in so callers
// looping over input bytes can reuse one heap buffer across steps instead of
// reallocating on every byte.
ErrorOr<ExprId> derivative_impl(ExprId root, char c, ExprPool &pool,
                                uint8_t ctx_before, DerivStack &stack) {
  if (!pool.is_valid_input(root))
    return Error(REG_BADPAT);

  // Reset len without freeing stack's heap buffer, discarding any frames left
  // by an earlier failed call.
  stack.clear();
  ExprId cur = root;
  ExprId res = INVALID_EXPR_ID;
  do {
    // Walk down first children until we reach a leaf.
    while (res == INVALID_EXPR_ID) {
      const Expr e = pool.get(cur);
      ExprId next_child = INVALID_EXPR_ID;
      // ExprPool interns children before their parent, so ids shrink on the way
      // down: the walk always reaches a leaf, and the stack never holds more
      // frames than the pool has nodes.
      switch (e.kind) {
      case ExprKind::EmptySet:
      case ExprKind::EmptyStr:
      case ExprKind::Anchor:
        res = EMPTY_SET_ID;
        break;
      case ExprKind::Literal:
        res = (c == e.ch) ? EMPTY_STR_ID : EMPTY_SET_ID;
        break;
      case ExprKind::ByteSet:
        res = pool.get_byte_set(e.byte_set_id).test(static_cast<uint8_t>(c))
                  ? EMPTY_STR_ID
                  : EMPTY_SET_ID;
        break;
      case ExprKind::Concat:
      case ExprKind::Alt:
        LIBC_ASSERT(e.bin.left < cur && e.bin.right < cur);
        next_child = e.bin.left;
        break;
      case ExprKind::Star:
        LIBC_ASSERT(e.body < cur);
        next_child = e.body;
        break;
      case ExprKind::Repeat:
        LIBC_ASSERT(e.rep.body < cur);
        next_child = e.rep.body;
        break;
      }
      if (next_child != INVALID_EXPR_ID) {
        if (!stack.push(DerivFrame(cur)))
          return Error(REG_ESPACE);
        cur = next_child;
      }
    }

    // Combine child results back up the stack until a parent needs its right
    // child walked (res is reset to INVALID_EXPR_ID) or the stack empties.
    while (!stack.empty() && res != INVALID_EXPR_ID) {
      DerivFrame &frame = stack.top();
      if (frame.d_left != INVALID_EXPR_ID) {
        // Both children of a binary Concat or Alt finished; union their terms.
        LIBC_ASSERT(pool.get(frame.id).kind == ExprKind::Concat ||
                    pool.get(frame.id).kind == ExprKind::Alt);
        ErrorOr<ExprId> combined = pool.make_alt(frame.d_left, res);
        if (!combined.has_value())
          return combined;
        res = *combined;
      } else {
        // First child finished; either combine now or save the left term and
        // walk the right child.
        const Expr pe = pool.get(frame.id);
        switch (pe.kind) {
        case ExprKind::Concat: {
          // Product Rule:
          // D_c(L . R) = (D_c(L) . R) | (nullable(L, ctx_before) ? D_c(R) : Ø)
          ErrorOr<ExprId> lhs = pool.make_concat(res, pe.bin.right);
          if (!lhs.has_value())
            return lhs;
          if (nullable(pe.bin.left, ctx_before, pool)) {
            LIBC_ASSERT(*lhs != INVALID_EXPR_ID);
            frame.d_left = *lhs;
            cur = pe.bin.right;
            res = INVALID_EXPR_ID;
            continue;
          }
          res = *lhs;
          break;
        }
        case ExprKind::Alt:
          // Sum Rule: D_c(L | R) = D_c(L) | D_c(R)
          frame.d_left = res;
          cur = pe.bin.right;
          res = INVALID_EXPR_ID;
          continue;
        case ExprKind::Star: {
          // Kleene Rule: D_c(R*) = D_c(R) . R*
          ErrorOr<ExprId> combined = pool.make_concat(res, frame.id);
          if (!combined.has_value())
            return combined;
          res = *combined;
          break;
        }
        case ExprKind::Repeat: {
          // Bounded Repetition Rule:
          // D_c(R{m,n}) = D_c(R) . R{nullable(R, ctx_before) ? 0 : max(0,m-1),
          //                          n-1}.
          // When R is nullable at ctx_before (e.g. via '^'), up to m - 1
          // leading copies may match empty before the copy consuming c.
          LIBC_ASSERT(pe.rep.max >= 1 && pe.rep.min <= pe.rep.max);
          if (res != EMPTY_SET_ID) {
            uint16_t next_min =
                (pe.rep.min > 1 && !nullable(pe.rep.body, ctx_before, pool))
                    ? static_cast<uint16_t>(pe.rep.min - 1)
                    : static_cast<uint16_t>(0);
            uint16_t next_max = (pe.rep.max == REPEAT_INF)
                                    ? REPEAT_INF
                                    : static_cast<uint16_t>(pe.rep.max - 1);
            ErrorOr<ExprId> tail =
                pool.make_repeat(pe.rep.body, next_min, next_max);
            if (!tail.has_value())
              return tail;
            ErrorOr<ExprId> combined = pool.make_concat(res, *tail);
            if (!combined.has_value())
              return combined;
            res = *combined;
          }
          break;
        }
        case ExprKind::EmptySet:
        case ExprKind::EmptyStr:
        case ExprKind::Literal:
        case ExprKind::Anchor:
        case ExprKind::ByteSet:
          LIBC_ASSERT(false);
          __builtin_unreachable();
        }
      }
      stack.pop();
    }
  } while (res == INVALID_EXPR_ID);
  LIBC_ASSERT(stack.empty() && res != INVALID_EXPR_ID);
  return res;
}

struct MatchCtx {
  ExprPool &pool;
  cpp::string_view input;
  int cflags;
  int eflags;
  DerivStack &stack;
};

struct FirstMatchEnd {
  MatchResult status = MatchResult::NoMatch;
  size_t end_pos = 0;
  ExprId active_state = EMPTY_SET_ID;
  uint8_t end_ctx = 0;
};

// Reverses a leaf expression directly, or returns INVALID_EXPR_ID for compound
// nodes that require stack traversal.
ErrorOr<ExprId> try_reverse_leaf(ExprId cur, ExprPool &pool) {
  const Expr e = pool.get(cur);
  switch (e.kind) {
  case ExprKind::EmptySet:
  case ExprKind::EmptyStr:
  case ExprKind::Literal:
  case ExprKind::ByteSet:
    return cur;
  case ExprKind::Anchor: {
    const uint8_t swapped = swap_anchors(e.anchor_mask);
    return (swapped == e.anchor_mask) ? cur : pool.make_anchor(swapped);
  }
  case ExprKind::Concat:
  case ExprKind::Alt:
  case ExprKind::Star:
  case ExprKind::Repeat:
    return INVALID_EXPR_ID;
  }
  LIBC_ASSERT(false);
  __builtin_unreachable();
}

// Pass 1a: unanchored forward scan to the earliest match end E1.
// On Match, active_state holds derivative threads started strictly before
// end_pos (without unioning pattern at end_pos).
FirstMatchEnd find_first_match_end(const MatchCtx &mc, ExprId pattern) {
  FirstMatchEnd out;
  if (!mc.pool.is_valid_input(pattern) || pattern == EMPTY_SET_ID)
    return out;
  uint8_t ctx = ctx_at(mc.input, 0, mc.cflags, mc.eflags);
  if (nullable(pattern, ctx, mc.pool)) {
    out.status = MatchResult::Match;
    out.end_ctx = ctx;
    return out;
  }

  // Unanchored substring search: at each byte, step active partial matches via
  // derivative_impl() and union with pattern so a new match can begin at the
  // next fencepost (equivalent to matching .* . pattern without constructing a
  // prefix wildcard node).
  ExprId state = pattern;
  for (size_t pos = 0; pos < mc.input.size(); ++pos) {
    ErrorOr<ExprId> d =
        derivative_impl(state, mc.input[pos], mc.pool, ctx, mc.stack);
    if (!d.has_value()) {
      LIBC_ASSERT(d.error() == REG_ESPACE);
      out.status = MatchResult::OutOfMemory;
      return out;
    }
    ctx = ctx_at(mc.input, pos + 1, mc.cflags, mc.eflags);
    if (nullable(*d, ctx, mc.pool) || nullable(pattern, ctx, mc.pool)) {
      out.status = MatchResult::Match;
      out.end_pos = pos + 1;
      out.active_state = *d;
      out.end_ctx = ctx;
      return out;
    }
    ErrorOr<ExprId> next = mc.pool.make_alt(pattern, *d);
    if (!next.has_value()) {
      LIBC_ASSERT(next.error() == REG_ESPACE);
      out.status = MatchResult::OutOfMemory;
      return out;
    }
    state = *next;
  }
  return out;
}

// Pass 1b: extend active threads started before e1 (without adding new starts)
// until they die, finding the farthest end D any earlier-starting match can
// reach.
ErrorOr<size_t> find_match_horizon(const MatchCtx &mc, ExprId state, size_t e1,
                                   uint8_t ctx) {
  size_t d_horizon = e1;
  for (size_t pos = e1; pos < mc.input.size(); ++pos) {
    if (state == EMPTY_SET_ID || state == EMPTY_STR_ID)
      break;
    ErrorOr<ExprId> d =
        derivative_impl(state, mc.input[pos], mc.pool, ctx, mc.stack);
    if (!d.has_value()) {
      LIBC_ASSERT(d.error() == REG_ESPACE);
      return Error(d.error());
    }
    state = *d;
    ctx = ctx_at(mc.input, pos + 1, mc.cflags, mc.eflags);
    if (nullable(state, ctx, mc.pool))
      d_horizon = pos + 1;
  }
  return d_horizon;
}

// Pass 2: reverse scan from d_horizon down to 0 to find the leftmost start S*
// of any match ending in [e1, d_horizon].  Because e1 is the earliest match
// end, no match can end below e1: new reversed threads are only seeded at
// positions >= e1, and the scan stops once active threads die below e1.
ErrorOr<size_t> find_leftmost_start(const MatchCtx &mc, ExprId reversed_pattern,
                                    size_t e1, size_t d_horizon) {
  LIBC_ASSERT(e1 >= 1 && e1 <= d_horizon && d_horizon <= mc.input.size());
  size_t s_star = ~size_t(0);
  ExprId rev_state = reversed_pattern;
  uint8_t rctx = rev_ctx_at(mc.input, d_horizon, mc.cflags, mc.eflags);
  if (nullable(rev_state, rctx, mc.pool))
    s_star = d_horizon;

  for (size_t pos = d_horizon; pos > 0; --pos) {
    const size_t prev = pos - 1;
    ErrorOr<ExprId> d =
        derivative_impl(rev_state, mc.input[prev], mc.pool, rctx, mc.stack);
    if (!d.has_value()) {
      LIBC_ASSERT(d.error() == REG_ESPACE);
      return Error(d.error());
    }
    rctx = rev_ctx_at(mc.input, prev, mc.cflags, mc.eflags);
    if (prev >= e1) {
      if (nullable(*d, rctx, mc.pool) ||
          nullable(reversed_pattern, rctx, mc.pool))
        s_star = prev;
      ErrorOr<ExprId> next = mc.pool.make_alt(reversed_pattern, *d);
      if (!next.has_value()) {
        LIBC_ASSERT(next.error() == REG_ESPACE);
        return Error(next.error());
      }
      rev_state = *next;
    } else {
      if (nullable(*d, rctx, mc.pool))
        s_star = prev;
      rev_state = *d;
      if (rev_state == EMPTY_SET_ID || rev_state == EMPTY_STR_ID)
        break;
    }
  }
  LIBC_ASSERT(s_star <= e1);
  return s_star;
}

// Pass 3: anchored forward scan from s_star to find the longest match end E*.
ErrorOr<size_t> find_longest_end(const MatchCtx &mc, ExprId pattern,
                                 size_t s_star) {
  LIBC_ASSERT(s_star <= mc.input.size());
  ExprId fwd_state = pattern;
  uint8_t ctx = ctx_at(mc.input, s_star, mc.cflags, mc.eflags);
  size_t e_star = nullable(fwd_state, ctx, mc.pool) ? s_star : ~size_t(0);

  for (size_t pos = s_star; pos < mc.input.size(); ++pos) {
    if (is_universal_star(fwd_state, mc.pool)) {
      e_star = mc.input.size();
      break;
    }
    if (fwd_state == EMPTY_STR_ID)
      break;
    ErrorOr<ExprId> d =
        derivative_impl(fwd_state, mc.input[pos], mc.pool, ctx, mc.stack);
    if (!d.has_value()) {
      LIBC_ASSERT(d.error() == REG_ESPACE);
      return Error(d.error());
    }
    fwd_state = *d;
    if (fwd_state == EMPTY_SET_ID)
      break;
    ctx = ctx_at(mc.input, pos + 1, mc.cflags, mc.eflags);
    if (nullable(fwd_state, ctx, mc.pool))
      e_star = pos + 1;
  }
  LIBC_ASSERT(e_star >= s_star && e_star <= mc.input.size());
  return e_star;
}

} // namespace

ErrorOr<ExprId> derivative(ExprId id, char c, ExprPool &pool,
                           uint8_t ctx_before) {
  DerivStack stack;
  return derivative_impl(id, c, pool, ctx_before, stack);
}

ErrorOr<ExprId> reverse_expr(ExprId root, ExprPool &pool) {
  if (!pool.is_valid_input(root))
    return Error(REG_BADPAT);
  ErrorOr<ExprId> leaf = try_reverse_leaf(root, pool);
  if (!leaf.has_value() || *leaf != INVALID_EXPR_ID)
    return leaf;

  const size_t orig_node_count = pool.size();
  LIBC_ASSERT(root != INVALID_EXPR_ID && root <= orig_node_count);

  // Temporary memo table indexed by original ExprId keeps reversal O(|DAG|) on
  // shared non-Concat sub-expressions such as grammar productions, and is freed
  // on return.
  static_assert(INVALID_EXPR_ID == 0);
  auto memo_alloc = make_unique<ExprId[]>(orig_node_count + 1);
  if (!memo_alloc)
    return Error(REG_ESPACE);
  cpp::unique_ptr<ExprId[]> memo = memo_alloc.take_unique();

  RevStack stack;
  ExprId cur = root;
  ExprId res = INVALID_EXPR_ID;
  do {
    while (res == INVALID_EXPR_ID) {
      LIBC_ASSERT(cur != INVALID_EXPR_ID && cur <= orig_node_count);
      if (memo[cur] != INVALID_EXPR_ID) {
        res = memo[cur];
        break;
      }
      ErrorOr<ExprId> cur_leaf = try_reverse_leaf(cur, pool);
      if (!cur_leaf.has_value())
        return cur_leaf;
      if (*cur_leaf != INVALID_EXPR_ID) {
        res = *cur_leaf;
        memo[cur] = res;
        break;
      }
      const Expr e = pool.get(cur);
      ExprId next_child = INVALID_EXPR_ID;
      ExprId initial_acc = INVALID_EXPR_ID;
      ExprId initial_tail = INVALID_EXPR_ID;
      // ExprPool interns children before their parent, so ids shrink on the way
      // down: every walk terminates and stack depth stays below MAX_NODE_LIMIT.
      switch (e.kind) {
      case ExprKind::Concat:
        LIBC_ASSERT(e.bin.left < cur && e.bin.right < cur);
        next_child = e.bin.left;
        initial_acc = EMPTY_STR_ID;
        initial_tail = e.bin.right;
        break;
      case ExprKind::Alt:
        LIBC_ASSERT(e.bin.left < cur && e.bin.right < cur);
        next_child = e.bin.left;
        break;
      case ExprKind::Star:
        LIBC_ASSERT(e.body < cur);
        next_child = e.body;
        break;
      case ExprKind::Repeat:
        LIBC_ASSERT(e.rep.body < cur);
        next_child = e.rep.body;
        break;
      case ExprKind::EmptySet:
      case ExprKind::EmptyStr:
      case ExprKind::Literal:
      case ExprKind::Anchor:
      case ExprKind::ByteSet:
        LIBC_ASSERT(false);
        __builtin_unreachable();
      }
      if (!stack.push(RevFrame(cur, initial_acc, initial_tail)))
        return Error(REG_ESPACE);
      cur = next_child;
    }

    while (!stack.empty() && res != INVALID_EXPR_ID) {
      RevFrame &frame = stack.top();
      const Expr pe = pool.get(frame.id);
      switch (pe.kind) {
      case ExprKind::Concat: {
        // Prepend each reversed atom onto frame.acc so a K-element
        // right-associated Concat chain builds K - 1 nodes in one stack frame.
        ErrorOr<ExprId> combined = pool.make_concat(res, frame.acc);
        if (!combined.has_value())
          return combined;
        if (frame.tail != INVALID_EXPR_ID) {
          const Expr te = pool.get(frame.tail);
          frame.acc = *combined;
          if (te.kind == ExprKind::Concat) {
            LIBC_ASSERT(te.bin.left < frame.tail && te.bin.right < frame.tail);
            cur = te.bin.left;
            frame.tail = te.bin.right;
          } else {
            cur = frame.tail;
            frame.tail = INVALID_EXPR_ID;
          }
          res = INVALID_EXPR_ID;
          continue;
        }
        res = *combined;
        break;
      }
      case ExprKind::Alt: {
        if (frame.acc == INVALID_EXPR_ID) {
          LIBC_ASSERT(res != INVALID_EXPR_ID);
          frame.acc = res;
          cur = pe.bin.right;
          res = INVALID_EXPR_ID;
          continue;
        }
        ErrorOr<ExprId> combined = pool.make_alt(frame.acc, res);
        if (!combined.has_value())
          return combined;
        res = *combined;
        break;
      }
      case ExprKind::Star: {
        ErrorOr<ExprId> combined = pool.make_star(res);
        if (!combined.has_value())
          return combined;
        res = *combined;
        break;
      }
      case ExprKind::Repeat: {
        ErrorOr<ExprId> combined =
            pool.make_repeat(res, pe.rep.min, pe.rep.max);
        if (!combined.has_value())
          return combined;
        res = *combined;
        break;
      }
      case ExprKind::EmptySet:
      case ExprKind::EmptyStr:
      case ExprKind::Literal:
      case ExprKind::Anchor:
      case ExprKind::ByteSet:
        LIBC_ASSERT(false);
        __builtin_unreachable();
      }
      memo[frame.id] = res;
      stack.pop();
    }
  } while (res == INVALID_EXPR_ID);
  LIBC_ASSERT(stack.empty() && res != INVALID_EXPR_ID);
  return res;
}

MatchResult match(ExprPool &pool, const ExprId pattern, cpp::string_view input,
                  int cflags, int eflags) {
  // One stack serves every byte, so a heap array allocated for one byte is
  // reused for the rest.
  DerivStack stack;
  const MatchCtx mc{pool, input, cflags, eflags, stack};
  return find_first_match_end(mc, pattern).status;
}

// TODO: Additional ExprPool Kleene/nullable subsumption rules (shrinking
// derivative state growth on long optional chains such as "(a?)^N b") and
// per-step DAG memoisation are deferred to follow-up changes.
MatchResult match_span(ExprPool &pool, const ExprId pattern,
                       const ExprId reversed_pattern, cpp::string_view input,
                       MatchSpan &span, int cflags, int eflags) {
  if (!pool.is_valid_input(pattern) || !pool.is_valid_input(reversed_pattern) ||
      pattern == EMPTY_SET_ID)
    return MatchResult::NoMatch;

  // One stack serves all passes, so any heap growth is reused across bytes.
  DerivStack stack;
  const MatchCtx mc{pool, input, cflags, eflags, stack};
  const FirstMatchEnd first = find_first_match_end(mc, pattern);
  if (first.status != MatchResult::Match)
    return first.status;

  size_t s_star = 0;
  if (first.end_pos > 0) {
    ErrorOr<size_t> d_horizon = find_match_horizon(
        mc, first.active_state, first.end_pos, first.end_ctx);
    if (!d_horizon.has_value())
      return MatchResult::OutOfMemory;
    ErrorOr<size_t> start =
        find_leftmost_start(mc, reversed_pattern, first.end_pos, *d_horizon);
    if (!start.has_value())
      return MatchResult::OutOfMemory;
    s_star = *start;
  }

  ErrorOr<size_t> e_star = find_longest_end(mc, pattern, s_star);
  if (!e_star.has_value())
    return MatchResult::OutOfMemory;

  span = {s_star, *e_star};
  return MatchResult::Match;
}

} // namespace regex
} // namespace LIBC_NAMESPACE_DECL
