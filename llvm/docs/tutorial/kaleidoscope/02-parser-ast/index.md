# Section 2: The Parser and AST (From Tokens to Trees)

```{toctree}
:hidden:

01-first-ast-node
02-parentheses
03-variables
04-function-calls
```

In [Section 1](../01-lexer/index.md), you built an interactive lexer that slices raw source text into structured tokens.  Now we need to assemble that flat stream of tokens into an **Abstract Syntax Tree (AST)**.

An AST is an in-memory tree representation of your program's structure.  Every downstream phase of our compiler (immediate evaluation in Section 3, LLVM IR generation in Section 4, and JIT compilation in Section 5) traverses this tree to understand what your code computes.

In this section, you will build an interactive **AST Pretty-Printer** in four focused steps, working directly inside the same `kaleidoscope.cpp` file from Section 1.  Within minutes, you will type expressions at a `ready> ` prompt and watch your compiler print clean S-expression syntax trees to your terminal.

---

## Steps in This Section

1.  [Step 2.1: The Base AST and Numbers](01-first-ast-node.md) - Define the `ExprAST` base class, `NumberExprAST`, `ParsePrimary()`, and an interactive REPL loop.
2.  [Step 2.2: Parentheses and Grouping](02-parentheses.md) - Group subexpressions recursively with `( ... )` and report syntax errors cleanly.
3.  [Step 2.3: Variables](03-variables.md) - Author `VariableExprAST` yourself and parse named variable references.
4.  [Step 2.4: Function Calls](04-function-calls.md) - Distinguish `x` from `foo(x, 10)` with one-token lookahead, plus an active learning challenge.

---

## The Living File Workflow

Keep your `kaleidoscope.cpp` file from [Step 1.5](../01-lexer/05-skipping-comments.md) open (making sure you reverted the temporary `if`/`then`/`else` challenge tokens!).  At each step, you will add a small, self-contained slice of C++, recompile, and test interactively.

Collapsible reference snapshots appear inside every step, and standalone files are also available in the [Reference Code Archive](../code/index.md).

---

[Previous Section: The Lexer](../01-lexer/index.md) | [Tutorial Overview](../index.md) | [Start: Step 2.1 - The Base AST and Numbers](01-first-ast-node.md) | [Next Section: Operator Precedence and Evaluation](../03-operators-eval/index.md)
