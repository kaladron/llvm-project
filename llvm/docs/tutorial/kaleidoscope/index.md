# Kaleidoscope: Building a Compiler with LLVM

## The Philosophy of This Tutorial

Most compiler tutorials fail for a simple reason: they make you wait too long before anything actually runs.  They hand you hundreds of lines of disconnected code snippets, ask you to trust that it will all make sense later, and leave you staring at compiler errors when the pieces do not fit together.

This tutorial takes the opposite approach.  You will work in a single living file, building in tiny steps of five to fifteen lines of code.  Each step takes between one and three minutes to type, compiles immediately, and gives you instant visual feedback in your terminal.

By the end of this series, you will have written a complete, working programming language with an interactive REPL, an optimising JIT compiler, and an ahead-of-time code generator that produces standalone native object files.

---

## The Ground Rules: Active Learning

If your goal is simply to have a working Kaleidoscope binary on your machine, you can check out the LLVM source tree and build the existing examples.  The goal of this tutorial is different.  The goal is for you to understand compilers.

To get the most out of this tutorial, commit to two rules:

1.  **Type everything by hand.**  Do not copy and paste blocks of code from the browser into your editor.  Typing code forces your eyes to scan every token, helps build muscle memory for compiler data structures, and forces you to confront syntax errors when you make a typo.
2.  **No LLMs.**  Do not paste these chapters into a large language model to generate the code for you.  Using a code generator robs you of the specific friction that produces learning.  When you encounter a compiler error or an unexpected assertion failure, read the diagnostic, inspect the state of your AST or token stream, and track it down yourself.

Throughout the tutorial, we will provide guided challenges where you add features to the language yourself.  Reference solutions are provided under expandable folds so that you can check your work after attempting the implementation.

---

## The Kaleidoscope Language

Kaleidoscope is a procedural language designed to teach compiler concepts without getting bogged down in type systems or complicated runtime machinery.

To keep things straightforward, the only primitive data type in Kaleidoscope is a 64-bit floating-point number (a `double` in C++).  Every expression evaluates to a double, and function parameters and return types are doubles.  Because of this, you do not need type annotations.

Here is what Kaleidoscope code looks like:

```text
# Compute the x'th Fibonacci number.
def fib(x)
  if x < 3 then
    1
  else
    fib(x - 1) + fib(x - 2);

# Evaluate the 10th Fibonacci number.
fib(10);
```

Kaleidoscope also supports calling external C standard library functions directly through the `extern` keyword:

```text
extern sin(x);
extern cos(x);

sin(0.5) * sin(0.5) + cos(0.5) * cos(0.5);
```

Over the course of this tutorial, we will start with basic arithmetic, add functions, introduce conditional branches and loops, implement mutable local variables, and compile everything to native machine code.

---

## Prerequisites & Environment Check

Before writing code, verify that you have a working C++ compiler and the LLVM development libraries installed.

Run the following command in your terminal:

```bash
llvm-config --version --cxxflags --ldflags --system-libs --libs core
```

If `llvm-config` is present and prints compiler flags along with library names, your environment is ready.  We will use Clang (`clang++`) throughout this guide.

---

## Tutorial Roadmap

### [Section 1: The Lexer (From Raw Text to Tokens)](01-lexer/index.md)
- [Step 1.1: Your First Token (The 90-Second Win)](01-lexer/01-first-token.md) - A fifteen-line character echo loop running in under two minutes.
- [Step 1.2: Skipping Whitespace](01-lexer/02-skipping-whitespace.md) - Teach the scanner to discard whitespace with character buffering.
- [Step 1.3: Numeric Literals](01-lexer/03-numeric-literals.md) - Read multi-digit integers and floating-point decimals.
- [Step 1.4: Identifiers and Keywords](01-lexer/04-identifiers-keywords.md) - Distinguish `def` and `extern` from variable and function names.
- [Step 1.5: Skipping Comments](01-lexer/05-skipping-comments.md) - Discard comments starting with `#`, plus an active learning challenge.

### [Section 2: The Parser and AST (From Tokens to Trees)](02-parser-ast/index.md)
- [Step 2.1: The Base AST and Numbers](02-parser-ast/01-first-ast-node.md) - Define the `ExprAST` base class, `NumberExprAST`, and parse numeric literals.
- [Step 2.2: Parentheses and Grouping](02-parser-ast/02-parentheses.md) - Group subexpressions with parentheses and catch syntax errors.
- [Step 2.3: Variables](02-parser-ast/03-variables.md) - Represent and parse named variable references.
- [Step 2.4: Function Calls](02-parser-ast/04-function-calls.md) - Parse function call expressions and comma-separated argument lists.

### [Section 3: Operator Precedence and The Calculator](03-operators-eval/index.md)
- [Step 3.1: The Binary AST Node](03-operators-eval/01-binary-ast.md) - Define `BinaryExprAST` with left and right child subtrees.
- [Step 3.2: The Precedence Table](03-operators-eval/02-precedence-table.md) - Configure operator binding strengths with a lookup table.
- [Step 3.3: Precedence Climbing](03-operators-eval/03-precedence-climbing.md) - Parse arithmetic expressions with operator precedence in eighteen lines.
- [Step 3.4: Immediate Evaluation](03-operators-eval/04-immediate-eval.md) - Add direct tree evaluation in pure C++ and turn your parser into a live calculator.

### [Section 4: Generating LLVM Intermediate Representation](04-llvm-ir/index.md)
- [Step 4.1: LLVM Setup and Constant IR](04-llvm-ir/01-llvm-setup.md) - Initialise the context, module, and builder, and emit numeric constants.
- [Step 4.2: Binary Math in IR](04-llvm-ir/02-binary-ir.md) - Generate floating-point math instructions (`fadd`, `fsub`, `fmul`, `fdiv`).
- [Step 4.3: Function Definitions](04-llvm-ir/03-functions-ir.md) - Define function prototypes, function bodies, and basic blocks.
- [Step 4.4: Function Calls and 'extern'](04-llvm-ir/04-calls-and-extern.md) - Call user functions and declare external C standard library functions.

### [Section 5: The JIT (Executing Native Machine Code)](05-jit/index.md)
- [Step 5.1: Native Target Initialisation](05-jit/01-target-setup.md) - Initialise the target machine for your host processor architecture.
- [Step 5.2: The JIT Engine](05-jit/02-jit-engine.md) - Set up the `KaleidoscopeJIT` instance and configure the module data layout.
- [Step 5.3: Executing Expressions](05-jit/03-executing-expressions.md) - Compile and execute top-level mathematical expressions natively in memory.
- [Step 5.4: Functions and Library Calls](05-jit/04-functions-and-externs.md) - Call user-defined functions and external C standard library functions from the JIT REPL.

### [Section 6: Control Flow (if / then / else)](06-control-flow/index.md)
- [Step 6.1: Lexing and Parsing 'if / then / else'](06-control-flow/01-if-ast.md) - Recognise conditional keywords and build the `IfExprAST` node.
- [Step 6.2: Basic Blocks and Branching](06-control-flow/02-basic-blocks-branching.md) - Divide code into single-entry blocks and emit conditional branches.
- [Step 6.3: SSA PHI Nodes](06-control-flow/03-phi-nodes.md) - Merge expression values across divergent control flow paths.
- [Step 6.4: Recursive Fibonacci in the JIT](06-control-flow/04-fibonacci-jit.md) - Run recursive control flow in your interactive JIT REPL.

### [Section 7: Loops and Iteration](07-loops/index.md)
- [Step 7.1: Lexing and Parsing 'for' Loops](07-loops/01-for-ast.md) - Add `for`, `=`, and `in` keywords, and parse loop syntax into `ForExprAST`.
- [Step 7.2: Emitting Loop Basic Blocks and PHI Nodes](07-loops/02-loop-codegen.md) - Generate the preheader, loop body, counter increment, and exit blocks.
- [Step 7.3: Terminal Graphics and Host Calls](07-loops/03-terminal-graphics.md) - Use `-rdynamic` to expose host I/O functions and draw patterns from the JIT.

### [Section 8: User-Defined Operators](08-user-defined-ops/index.md)
- [Step 8.1: Parsing Custom Operator Prototypes](08-user-defined-ops/01-custom-operators-ast.md) - Recognise `binary` and `unary` keywords in prototypes and capture user precedence.
- [Step 8.2: Unary Operators](08-user-defined-ops/02-unary-operators.md) - Parse prefix unary operators and emit function calls like `unary!(v)`.
- [Step 8.3: User Binary Operators](08-user-defined-ops/03-binary-operators.md) - Dynamically update `BinopPrecedence` and emit calls to `binary<op>(LHS, RHS)`.
- [Step 8.4: The Mandelbrot Set](08-user-defined-ops/04-mandelbrot.md) - Define standard library operators in Kaleidoscope and render the Mandelbrot fractal.

### [Section 9: Local Variables and Mutability](09-local-variables/index.md)
- [Step 9.1: Stack Allocation with 'alloca'](09-local-variables/01-alloca-stack-variables.md) - Create entry block `alloca` variables, read with `load`, and write with `store`.
- [Step 9.2: Local Variables and Assignment](09-local-variables/02-var-in-expressions.md) - Add `var / in` syntax and the assignment operator `=`.
- [Step 9.3: Automatic SSA Promotion with 'mem2reg'](09-local-variables/03-mem2reg-optimization.md) - Run LLVM's `mem2reg` pass to eliminate memory accesses.

### [Section 10: Compiling to Standalone Object Files](10-object-files/index.md)
- [Step 10.1: Configuring the Target Machine](10-object-files/01-target-machine.md) - Initialise target registries, query the host target triple, and configure `llvm::TargetMachine`.
- [Step 10.2: Emitting Object Code](10-object-files/02-emitting-object-code.md) - Run LLVM's code emission passes to write a native `.o` object file to disk.
- [Step 10.3: Linking Standalone Executables](10-object-files/03-linking-standalone-binary.md) - Write a C driver and link with `clang` to produce a standalone executable.
