# Section 2: The Parser and AST (From Tokens to Trees)

In [Section 1](../01-lexer/index.md), you created an interactive lexer that breaks raw text into structured tokens.  Now we need to assemble those tokens into an Abstract Syntax Tree (AST).

An AST is an in-memory tree representation of your program.  Later stages of our compiler - including code generation, optimisation, and JIT compilation - traverse this tree to understand what your code computes.

In this section, you will build an interactive **AST Pretty-Printer** in four small steps.  You will continue working directly in your `kaleidoscope.cpp` file, adding a few lines at a time.  Within minutes, you will type an expression and see your binary print a visual tree representation to your terminal.

---

## Steps in This Section

1.  [Step 2.1: The Base AST and Numbers](01-first-ast-node.md) - Define the `ExprAST` base class, `NumberExprAST`, and parse numeric literals.
2.  [Step 2.2: Parentheses and Grouping](02-parentheses.md) - Group subexpressions with `( ... )` and report syntax errors.
3.  [Step 2.3: Variables](03-variables.md) - Represent and parse named variable references.
4.  [Step 2.4: Function Calls](04-function-calls.md) - Parse function call expressions and comma-separated argument lists, plus an active learning challenge.

---

## The Living File Workflow

Keep your `kaleidoscope.cpp` file from Section 1 open.  At each step, you will add twenty or fewer lines of code, recompile, and run the binary to verify your progress.

Full reference implementations for each step are stored in the [code/](../code/) directory.
