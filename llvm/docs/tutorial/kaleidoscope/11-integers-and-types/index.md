# Section 11: Primitive Types, Integers, and Casts

```{toctree}
:hidden:

01-integer-widths
02-signless-integers
03-integer-comparisons
04-type-conversions
```

Welcome to Part 2 of the Kaleidoscope tutorial!  Across Sections 1 through 10, we made a deliberate pedagogical trade-off: every value in Kaleidoscope was a 64-bit IEEE 754 floating-point number (`double`).  Keeping the language uni-typed let you build a complete lexer, operator-precedence parser, SSA intermediate representation generator, ORCv2 JIT engine, `mem2reg` optimisation pipeline, and standalone `.o` object-file compiler without drowning in type-checking boilerplate.

Real systems software, however, cannot run on `double` alone.  Loop counters, byte buffers, bitmasks, exit codes, boolean conditions, and memory addresses require genuine **integers** of specific bit-widths.

Starting in this section, we evolve the exact `kaleidoscope.cpp` compiler you finished in [Step 10.3](../10-object-files/03-linking-standalone-binary.md) into a statically typed systems language.  Better yet, every `: <type>` parameter annotation and `-> <type>` return-type annotation is optional and defaults to `double` when omitted, so every Part 1 Kaleidoscope program continues to compile and run unchanged.

---

## LLVM Type Architecture

```text
                         +------------------+
                         |    llvm::Type    |
                         +------------------+
                                  |
       +-------------+------------+------------+--------------+
       |             |            |            |              |
       v             v            v            v              v
  +---------+   +---------+   +-------+   +---------+   +-----------+
  | Integer |   |  Float  |   |  Ptr  |   |  Void   |   | Aggregate |
  | i1, i8, |   |  half,  |   |  ptr  |   |  void   |   | [N x T],  |
  | i32,i64 |   | float,  |   |(opaque|   |(no value|   | { T1,T2 } |
  |  ...iN  |   | double  |   | addr) |   | return) |   |  (Sec 14) |
  +---------+   +---------+   +-------+   +---------+   +-----------+
```

:::{important}
**Zero Implicit Conversions in LLVM IR**:
LLVM Intermediate Representation enforces strict type matching on every instruction.  You cannot add an `i32` directly to an `i64`, pass an `i8` where an `i32` parameter is declared, or branch on a `double`.  Every width change or domain crossing must be expressed as an explicit LLVM cast or comparison instruction emitted by your frontend.
:::

---

## What You Will Build in This Section

1.  [Step 11.1: Integer Widths and Primitive Types](01-integer-widths.md): Add a frontend `KType` descriptor (`double`, `int`, `uint`, `i64`, `byte`, `bool`, `void`), lex integer and boolean literals separately from floating-point literals, parse optional `: <type>` and `-> <type>` annotations, and splice JIT top-level expressions so the REPL can print any return type.
2.  [Step 11.2: Signless Registers and Integer Arithmetic](02-signless-integers.md): Discover why LLVM's `i32` register has no signedness, widen binary operator tokens to support `<<` and `>>`, and emit two's-complement arithmetic, bitwise logic, and signed versus unsigned division and shifts.
3.  [Step 11.3: Integer Comparisons with `icmp`](03-integer-comparisons.md): Lex `==`, `!=`, `<=`, and `>=`, emit signed and unsigned `icmp` predicates alongside floating-point `fcmp` predicates, and return native `i1` booleans straight into `if` and `for` control flow.
4.  [Step 11.4: Explicit Type Conversions (`as`)](04-type-conversions.md): Parse postfix `expr as <type>` casts and lower all eight LLVM conversion paths (`sext`, `zext`, `trunc`, `sitofp`, `uitofp`, `fptosi`, `fptoui`, and zero-instruction `int` to `uint` reinterpretation).

---

## Compiling Your Living Compiler

Throughout Sections 11 through 13, compile your `kaleidoscope.cpp` file with the same command used at the end of Section 10:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete, buildable snapshots for every step live in the [code/index.md](../code/index.md) directory (`s11-01-integer-widths.cpp` through `s11-04-type-conversions.cpp`).

---

[Previous: Step 10.3](../10-object-files/03-linking-standalone-binary.md) | [Tutorial Index](../index.md) | [Start Step 11.1](01-integer-widths.md)
