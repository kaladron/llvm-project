# Section 7: Loops and Iteration

In [Section 6](../06-control-flow/index.md), you introduced conditional branching and recursive functions to Kaleidoscope.  While recursion allows repetition, many numerical and algorithmic tasks are most naturally expressed using loops.

In this section, you will add `for` loop expressions to Kaleidoscope.  You will learn how compilers structure loops in Static Single Assignment (SSA) form, how to use a PHI node as an iteration counter (induction variable), and how to connect native C host functions to draw text patterns directly from JIT-compiled code.

---

## Steps in This Section

1.  [Step 7.1: Lexing and Parsing 'for' Loops](01-for-ast.md) - Add `for`, `=`, and `in` keywords, and parse loop syntax into `ForExprAST`.
2.  [Step 7.2: Emitting Loop Basic Blocks and PHI Nodes](02-loop-codegen.md) - Generate the preheader, loop body, counter increment, and exit blocks.
3.  [Step 7.3: Terminal Graphics and Host Calls](03-terminal-graphics.md) - Use `-rdynamic` to expose host I/O functions and draw patterns from the JIT.

---

## Compiling Your Compiler

When calling C functions defined in your compiler's own source file (such as `putchard`), pass `-rdynamic` so the host dynamic loader exports their symbols into the JIT's search space:

```bash
clang++ -rdynamic -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Reference code implementations for each step are stored in the [code/](../code/) directory.
