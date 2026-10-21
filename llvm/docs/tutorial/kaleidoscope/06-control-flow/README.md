# Section 6: Control Flow (if / then / else)

In [Section 5](../05-jit/README.md), you constructed a native Just-In-Time (JIT) compiler capable of compiling mathematical expressions and functions to machine code.  However, your language remains strictly linear.  Without conditional branching, Kaleidoscope cannot make decisions or compute recursive algorithms.

In this section, you will introduce conditional control flow to Kaleidoscope.  You will implement `if / then / else` expressions, understand LLVM basic blocks, and learn how Static Single Assignment (SSA) form merges values with PHI nodes.

By the end of this section, your JIT will compile and execute recursive algorithms such as the Fibonacci sequence natively.

---

## Steps in This Section

1.  [Step 6.1: Lexing and Parsing 'if / then / else'](01-if-ast.md) - Recognise conditional keywords and build the `IfExprAST` node.
2.  [Step 6.2: Basic Blocks and Branching](02-basic-blocks-branching.md) - Divide code into single-entry blocks and emit conditional branches.
3.  [Step 6.3: SSA PHI Nodes](03-phi-nodes.md) - Merge expression values across divergent control flow paths.
4.  [Step 6.4: Recursive Fibonacci in the JIT](04-fibonacci-jit.md) - Run recursive control flow in your interactive JIT REPL.

---

## Compiling Your Compiler

As in Section 5, compile your living `kaleidoscope.cpp` against the LLVM core, ORC JIT, and native target libraries:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Reference code implementations for each step are stored in the [code/](../code/) directory.
