# Section 6: Control Flow (if / then / else)

```{toctree}
:hidden:

01-if-ast
02-basic-blocks-branching
03-phi-nodes
04-fibonacci-jit
```

In [Section 5](../05-jit/index.md), you built a Just-In-Time (JIT) compiler capable of compiling mathematical expressions and functions directly to native machine code.  Right now, however, every Kaleidoscope function is strictly linear: execution enters at the top and runs straight through to the bottom.  Without conditional branching, your language cannot make decisions or terminate recursive functions.

In this section, you will add conditional control flow to Kaleidoscope.  You will parse `if / then / else` expressions, split linear functions into LLVM **basic blocks**, and merge values from divergent branches in Static Single Assignment (SSA) form using **PHI nodes**.

By the end of this section, your interactive JIT REPL will compile and run recursive algorithms such as Fibonacci at native CPU speed.

---

## Steps in This Section

1.  [Step 6.1: Lexing and Parsing 'if / then / else'](01-if-ast.md) - Recognise conditional keywords, build `IfExprAST`, and wire up a clean `codegen()` stub.
2.  [Step 6.2: Basic Blocks and Branching](02-basic-blocks-branching.md) - Split control flow across `then`, `else`, and `ifcont` basic blocks and inspect the branching IR.
3.  [Step 6.3: SSA PHI Nodes](03-phi-nodes.md) - Merge branch values in SSA form using `llvm::PHINode`.
4.  [Step 6.4: Recursive Fibonacci in the JIT](04-fibonacci-jit.md) - Compile and execute recursive functions natively in your JIT REPL.

---

## Compiling Your Compiler

Throughout Section 6, compile your living `kaleidoscope.cpp` against the LLVM core, ORCv2 JIT, and native target libraries:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference implementations for every step are available in [code/index.md](../code/index.md).

---

[Previous: Section 5 - The JIT](../05-jit/index.md) | [Tutorial Home](../index.md) | [Next: Step 6.1 - Lexing and Parsing 'if / then / else'](01-if-ast.md)
