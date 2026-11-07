# Section 16: Writing a Custom LLVM Pass

```{toctree}
:hidden:

01-pass-pipeline
02-inspecting-modifying-ir
03-running-pass
```

Across the first fifteen sections of this tutorial, you used LLVM primarily from the **frontend** side: parsing source code, lowering types and AST nodes into SSA Intermediate Representation, attaching DWARF metadata, and handing modules to ORC JIT or `TargetMachine`.

Much of LLVM's enduring magic, however, lives in its **middle-end transformation and optimisation infrastructure**.  Every day, compiler engineers write modular **passes** that inspect IR, prove properties about control flow and values, and rewrite instructions into smaller, faster code.

In this final capstone section, you will write your own custom New Pass Manager optimisation pass (`KaleidoscopePeepholePass`), match algebraic and annihilation patterns with `llvm::PatternMatch`, safely rewrite SSA def-use chains, register a textual `"ks-peephole"` pipeline stage with `PassBuilder`, and watch redundant arithmetic collapse down to a single `ret` instruction!

---

## Custom Pass Pipeline Architecture

```text
  AST Codegen (alloca / load / store)
         |
         v
  +----------------------------------------------------------+
  | FunctionPassManager ("mem2reg,ks-peephole")              |
  |                                                          |
  |   1. PromotePass (mem2reg)                               |
  |      Lifts stack allocas into clean SSA registers        |
  |            |                                             |
  |            v                                             |
  |   2. KaleidoscopePeepholePass ("ks-peephole")            |
  |      Folds x + 0, x - 0, x * 1, x - x, and x * 0         |
  |      Preserves CFGAnalyses in FunctionAnalysisManager    |
  +----------------------------------------------------------+
         |
         v
  llvm::verifyFunction  --->  ORC JIT Execution / ELF .o File
```

:::{note}
**The Modern New Pass Manager (NPM)**:
Older LLVM tutorials taught the legacy `llvm::FunctionPass` class hierarchy and static `RegisterPass` constructors.  Modern LLVM uses a value-semantic, CRTP mixin architecture (`PassInfoMixin` / `OptionalPassInfoMixin`) paired with explicit analysis managers (`FunctionAnalysisManager`).  Passes are plain lightweight C++ structs with a `run(Function &, FunctionAnalysisManager &)` method, enabling fine-grained analysis caching and zero-overhead composition.
:::

---

## Steps in This Section

1.  [Step 16.1: The Modern LLVM Pass Pipeline](01-pass-pipeline.md) - Write a `KaleidoscopePeepholePass` skeleton using LLVM's pass mixin, add `--run-peephole`, and inspect unsimplified SSA functions after `mem2reg`.
2.  [Step 16.2: Inspecting and Modifying IR](02-inspecting-modifying-ir.md) - Safely mutate basic blocks with `make_early_inc_range`, match commutative patterns with `llvm::PatternMatch`, rewire SSA uses, and preserve `CFGAnalyses`.
3.  [Step 16.3: Registering and Running Your Pass](03-running-pass.md) - Register `"ks-peephole"` in `PassBuilder`, compare before-and-after IR side by side, verify transformed functions, and celebrate completing your Kaleidoscope compiler!

---

## Compiling Your Compiler

Compile `kaleidoscope.cpp` with the standard Part 2 build command:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference snapshots for each step are indexed in [code/index.md](../code/index.md).

---

[Previous: Source-Level Debugging with GDB and LLDB](../15-debug-info/03-source-level-debugging.md) | [Tutorial Overview](../index.md) | [Start Step 16.1](01-pass-pipeline.md)
