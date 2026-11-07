# Section 3: Operator Precedence and Evaluation

```{toctree}
:hidden:

01-binary-ast
02-precedence-table
03-precedence-climbing
04-immediate-eval
```

In [Section 2](../02-parser-ast/index.md), you built an Abstract Syntax Tree and recursive-descent parser for primary expressions: numbers, parentheses, variables, and function calls.

Now we need to parse binary infix operators such as `+`, `-`, `*`, `/`, and `<`.  Parsing binary operators is trickier than parsing primary atoms because operators bind with different strengths: in `1 + 2 * 3`, multiplication binds tighter than addition, so the tree must group as `1 + (2 * 3)` rather than `(1 + 2) * 3`.

In this section, you will first see that precedence bug happen live with a naive left-to-right loop, then solve it cleanly in twenty-five lines of C++ using **Operator Precedence Climbing**.  To cap off Milestone 1, you will add an `eval()` method to the AST, turning your frontend into an interactive floating-point calculator before we write a single line of LLVM code!

---

## Steps in This Section

1.  [Step 3.1: The Binary AST Node](01-binary-ast.md) - Author `BinaryExprAST` and wire up a naive left-to-right scan so you can watch the precedence bug firsthand.
2.  [Step 3.2: The Precedence Table](02-precedence-table.md) - Replace hardcoded operator checks with a data-driven `BinopPrecedence` lookup table.
3.  [Step 3.3: Precedence Climbing](03-precedence-climbing.md) - Fix grouping with recursive precedence climbing (`ParseBinOpRHS`).
4.  [Step 3.4: Immediate Evaluation](04-immediate-eval.md) - Walk the AST in pure C++ to compute live mathematical results and complete Milestone 1.

---

## The Living File Workflow

Keep working directly inside your `kaleidoscope.cpp` file from [Step 2.4](../02-parser-ast/04-function-calls.md) (with the compact S-expression `print()` format in place).  At each step, you will add a focused block of code, recompile, and test interactively in your terminal.

Collapsible reference snapshots appear inside every step, and standalone files are stored in the [Reference Code Archive](../code/index.md).

---

[Previous Section: The Parser and AST](../02-parser-ast/index.md) | [Tutorial Overview](../index.md) | [Start: Step 3.1 - The Binary AST Node](01-binary-ast.md) | [Next Section: Generating LLVM IR](../04-llvm-ir/index.md)
