# Section 14: Aggregate Structs, Arrays, and GetElementPtr

```{toctree}
:hidden:

01-struct-types-layout
02-understanding-gep
03-arrays-indexing
```

In Section 13, you taught Kaleidoscope how to work with opaque pointers, take addresses of variables, and allocate raw byte buffers on the heap with `malloc` and `free`.  Real systems programs, however, rarely allocate solitary integers or doubles in isolation.  They group related fields into **structs** and lay out contiguous sequences of elements in **arrays**.

In this section, you will evolve `kaleidoscope.cpp` to support user-defined `struct` types, `.field` member access, and fixed-size `[N x T]` arrays.  Along the way, you will see how LLVM's `DataLayout` computes hardware ABI alignment and padding, and you will master **`GetElementPtr` (GEP)**: the single most important, and most frequently misunderstood, instruction in the LLVM instruction set.

---

## Struct Layout and Address Calculation

```text
  struct Point { x: int, y: double }  (size = 16 bytes, align = 8 bytes)

  Byte offset:  +0        +4              +8                       +16
                +---------+---------------+------------------------+
                | x (i32) |  [4B padding] |       y (double)       |
                +---------+---------------+------------------------+
                ^                         ^
                Base ptr %p               getelementptr %Point, ptr %p, i32 0, i32 1
```

:::{important}
**The Golden Rule of GEP**:
The `getelementptr` instruction NEVER accesses memory.  It performs zero loads and zero stores.  GEP is purely an address calculation instruction: it takes a base pointer and one or more type-guided indices, computes a byte offset in registers using the module's `DataLayout`, and returns a new `ptr`.
:::

---

## Steps in This Section

1.  [Step 14.1: Struct Types and ABI DataLayout](01-struct-types-layout.md) - Parse `struct Name { ... }` definitions, build `llvm::StructType` across JIT contexts, and inspect hardware alignment padding with `StructLayout`.
2.  [Step 14.2: Demystifying GetElementPtr](02-understanding-gep.md) - Understand GEP's leading zero index and implement postfix `.field` access on both stack structs and heap pointers.
3.  [Step 14.3: Fixed-Size Arrays and Subscript Indexing](03-arrays-indexing.md) - Add `[N x T]` array types and `base[idx]` subscripting, contrasting two-index array GEPs against single-index pointer GEPs.

---

## Compiling Your Compiler

Every step in this section builds directly on your single `kaleidoscope.cpp` source file using the standard Part 2 build command:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference snapshots for each step are indexed in [code/index.md](../code/index.md).

---

[Previous: Heap Memory (malloc, free, and sizeof)](../13-pointers-and-heap/03-heap-malloc-free.md) | [Tutorial Overview](../index.md) | [Start Step 14.1](01-struct-types-layout.md)
