# Section 7: Loops and Iteration

```{toctree}
:hidden:

01-for-ast
02-loop-codegen
03-terminal-graphics
```

In [Section 6](../06-control-flow/index.md), you gave Kaleidoscope conditional `if / then / else` expressions and ran recursive Fibonacci on bare metal.  While recursion can express any repetition, writing explicit loops is far more natural for numerical stepping, grid traversal, and rendering output.

In this section, you will add `for ... in` loop expressions to Kaleidoscope.  You will see how compilers build cyclic Control Flow Graphs in pure Static Single Assignment (SSA) form using an induction-variable PHI node, and you will expose C helper functions from your host compiler binary so JIT-compiled loops can draw ASCII graphics directly in your terminal.

---

## Steps in This Section

1.  [Step 7.1: Lexing and Parsing 'for' Loops](01-for-ast.md) - Recognise the `for` and `in` keywords, build `ForExprAST`, and parse optional step expressions.
2.  [Step 7.2: Emitting Loop Basic Blocks and PHI Nodes](02-loop-codegen.md) - Lower `ForExprAST` into cyclic LLVM basic blocks with a `PHINode` loop counter and lexical variable shadowing.
3.  [Step 7.3: Terminal Graphics and Host Calls](03-terminal-graphics.md) - Export `putchard` and `printd` with `extern "C"` and `-rdynamic` to render terminal patterns from JIT machine code.

---

## Compiling Your Compiler

Starting in Section 7, pass `-rdynamic` when compiling `kaleidoscope.cpp` so the host linker exports C helper functions (such as `putchard` and `printd` in [Step 7.3](03-terminal-graphics.md)) into the dynamic symbol table where ORCv2 can discover them at runtime:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference implementations for every step are available in [code/index.md](../code/index.md).

---

[Previous: Section 6 - Control Flow](../06-control-flow/index.md) | [Tutorial Home](../index.md) | [Next: Step 7.1 - Lexing and Parsing 'for' Loops](01-for-ast.md)
