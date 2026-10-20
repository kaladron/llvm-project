# Section 3: Operator Precedence and The Calculator

In [Section 2](../02-parser-ast/index.md), you created an Abstract Syntax Tree that parses primary expressions, parentheses, variables, and function calls.

Now we need to parse binary operators like `+`, `-`, `*`, and `/`.  Handling binary operators is subtle because operators have different mathematical precedence.  In `1 + 2 * 3`, multiplication binds more tightly than addition.  The expression must be parsed as `1 + (2 * 3)`, not `(1 + 2) * 3`.

In this section, you will implement **Operator Precedence Climbing**.  To give you an immediate win, you will also add an `eval()` method directly to the AST.  By the end of this section, your binary will function as an interactive mathematical calculator in pure C++, computing arithmetic results live in your terminal before we even touch LLVM.

---

## Steps in This Section

1.  [Step 3.1: The Binary AST Node](01-binary-ast.md) - Define `BinaryExprAST` with left and right child subtrees.
2.  [Step 3.2: The Precedence Table](02-precedence-table.md) - Configure operator binding strengths with a lookup table.
3.  [Step 3.3: Precedence Climbing](03-precedence-climbing.md) - The precedence climbing algorithm in eighteen lines of C++.
4.  [Step 3.4: Immediate Evaluation](04-immediate-eval.md) - Add direct tree evaluation in pure C++ and turn your parser into a live calculator, plus an active learning challenge.

---

## The Living File Workflow

Continue working directly in your `kaleidoscope.cpp` file.  At each step, you will add a small block of code, recompile, and verify the new capability in your terminal.

Full reference implementations for each step are stored in the [code/](../code/) directory.
