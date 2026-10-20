# Kaleidoscope: Building a Compiler with LLVM

## The Philosophy of This Tutorial

Most compiler tutorials fail for a simple reason: they make you wait too long before anything actually runs.  They hand you hundreds of lines of disconnected code snippets, ask you to trust that it will all make sense later, and leave you staring at compiler errors when the pieces do not fit together.

This tutorial takes the opposite approach.  Every chapter in this series provides complete, compilable code with immediate payoff.  Even within a single chapter, you will build in tiny increments - writing twenty or thirty lines at a time, compiling immediately, and verifying that the code works before moving forward.

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

If `llvm-config` is present and prints compiler flags along with library names, your environment is ready.  We will use Clang (`clang++`) with C++17 throughout this guide.

---

## Chapter Roadmap

- [Chapter 1: The Lexer and Interactive Token Inspector](01-lexer.md) - Break raw text into structured tokens with a live, interactive token scanner.
- [Chapter 2: The Abstract Syntax Tree and Tree Pretty-Printing](02-ast-and-parsing.md) - Define AST nodes for numbers and variables, and pretty-print them.
- [Chapter 3: Binary Operators and Direct Evaluation](03-operators-and-eval.md) - Implement operator precedence climbing, and evaluate math expressions immediately in C++.
- [Chapter 4: Generating LLVM Intermediate Representation](04-llvm-ir.md) - Emit LLVM IR for expressions using the LLVM IR builder.
- [Chapter 5: The JIT and Native Execution](05-jit.md) - Compile LLVM IR into machine code in memory and execute it on the host CPU.
- [Chapter 6: Functions and External Library Calls](06-functions.md) - Support function definitions, arguments, and calls to standard library functions.
- [Chapter 7: Conditionals and Control Flow](07-control-flow.md) - Add `if/then/else` expressions using basic blocks and LLVM PHI nodes.
- [Chapter 8: Loops and Iteration](08-loops.md) - Add iterative `for` loops in SSA form.
- [Chapter 9: Local Variables and Mutability](09-variables.md) - Implement mutable local variables using stack allocation and memory-to-register promotion.
- [Chapter 10: Compiling to Standalone Object Files](10-object-code.md) - Emit `.o` object files and link them into native executable binaries.
