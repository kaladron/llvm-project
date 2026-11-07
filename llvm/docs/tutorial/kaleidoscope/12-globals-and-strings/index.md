# Section 12: Global Variables and Constant Strings

```{toctree}
:hidden:

01-global-variables
02-string-literals
03-calling-clib
```

In Section 11, you gave Kaleidoscope fixed-width integer types, booleans, bitwise arithmetic, comparisons, and explicit casts.  Yet every variable in your programs still vanished the instant its enclosing function returned, because `alloca` allocates storage on the function's call stack frame.

Systems programs also rely on **module-level memory segments** whose lifetimes span the entire process:

* Mutable global variables shared across function calls (`.data` and `.bss`).
* Null-terminated string literals stored in read-only constant pools (`.rodata`).
* Variadic foreign-function calls into the host C standard library (`puts`, `printf`).

---

## Process Memory Segments

```text
  High Addresses  +---------------------------------------------------+
                  |  Stack   (alloca: local variables & call frames)  |
                  |    |     grows downward, freed on function return |
                  |    v                                              |
                  |    ^                                              |
                  |    |     grows upward via malloc / free (Sec 13)  |
                  |  Heap    (dynamic runtime buffers)                |
                  +---------------------------------------------------+
                  |  .bss    (zero-initialised mutable globals)       |
                  +---------------------------------------------------+
                  |  .data   (non-zero initialised mutable globals)   |
                  +---------------------------------------------------+
                  |  .rodata (read-only constants & string literals)  |
                  +---------------------------------------------------+
  Low Addresses   |  .text   (compiled native machine instructions)   |
                  +---------------------------------------------------+
```

:::{note}
**Static Lifetimes vs. Stack Frames**:
Unlike `alloca` variables, an `llvm::GlobalVariable` lives at a fixed virtual address for the lifetime of the program (or until its `ResourceTracker` is unloaded in an interactive ORCv2 JIT session).  Zero-initialised globals placed in `.bss` occupy zero bytes in your `.o` object file on disk; the operating system kernel maps fresh zeroed pages when the program starts.
:::

---

## What You Will Build in This Section

1.  [Step 12.1: Global Variables Across JIT Modules](01-global-variables.md): Add `global name: type = init;`, manage ORCv2 `GlobalRTs` resource trackers alongside external declarations in `getGlobal()`, and wire global loads and stores seamlessly through `VariableExprAST::codegenAddress()`.
2.  [Step 12.2: String Literals and Constant Pools](02-string-literals.md): Lex double-quoted string literals with escape sequences, introduce pointer types (`ptr`), and emit deduplicated null-terminated byte arrays into `.rodata` with `Builder->CreateGlobalString()`.
3.  [Step 12.3: Variadic Functions and Calling `libc`](03-calling-clib.md): Lex `...` in prototypes, flip `FunctionType::get(..., IsVarArg)` to `true`, implement C's default variadic argument promotions, and call `puts` and `printf` live from the Kaleidoscope REPL.

---

## Compiling Your Living Compiler

Compile `kaleidoscope.cpp` with the standard build command:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference snapshots for every step live in [code/index.md](../code/index.md) (`s12-01-global-variables.cpp` through `s12-03-calling-clib.cpp`).

---

[Previous: Step 11.4](../11-integers-and-types/04-type-conversions.md) | [Tutorial Index](../index.md) | [Start Step 12.1](01-global-variables.md)
