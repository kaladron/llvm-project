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

If `llvm-config` is present and prints compiler flags along with library names, your environment is ready.  We will use Clang (`clang++`) with C++17 throughout this guide.

---

## Tutorial Roadmap

### [Section 1: The Lexer (From Raw Text to Tokens)](01-lexer/index.md)
- [Step 1.1: Your First Token (The 90-Second Win)](01-lexer/01-first-token.md) - A fifteen-line character echo loop running in under two minutes.
- [Step 1.2: Skipping Whitespace](01-lexer/02-skipping-whitespace.md) - Teach the scanner to discard whitespace with character buffering.
- [Step 1.3: Numeric Literals](01-lexer/03-numeric-literals.md) - Read multi-digit integers and floating-point decimals.
- [Step 1.4: Identifiers and Keywords](01-lexer/04-identifiers-keywords.md) - Distinguish `def` and `extern` from variable and function names.
- [Step 1.5: Skipping Comments](01-lexer/05-skipping-comments.md) - Discard comments starting with `#`, plus an active learning challenge.

### Section 2: The Parser and AST (From Tokens to Trees)
- Step 2.1: The Base AST and Numbers
- Step 2.2: Parentheses and Grouping
- Step 2.3: Variables
- Step 2.4: Function Calls

### Section 3: Operator Precedence and The Calculator
- Step 3.1: The Binary Node
- Step 3.2: The Precedence Table
- Step 3.3: Precedence Climbing
- Step 3.4: Immediate Evaluation

### Section 4: Generating LLVM Intermediate Representation
- Step 4.1: LLVM Context and Module
- Step 4.2: Emitting Constants
- Step 4.3: Binary Math IR
- Step 4.4: Variable Lookups

### Section 5: The JIT (Executing Native Machine Code)
- Step 5.1: Native Target Initialisation
- Step 5.2: Anonymous Top-Level Wrappers
- Step 5.3: In-Memory JIT Execution

### Section 6: Functions and External Library Calls
- Step 6.1: Function Prototypes
- Step 6.2: Function Bodies
- Step 6.3: External C Library Calls

### Section 7: Control Flow (if / then / else)
- Step 7.1: Conditional Expressions
- Step 7.2: Basic Blocks and Branching
- Step 7.3: SSA PHI Nodes

### Section 8: Loops and Iteration
- Step 8.1: The `for` Loop Syntax
- Step 8.2: Loop Basic Blocks and PHI Nodes
- Step 8.3: Generating ASCII Patterns

### Section 9: Local Variables and Mutability
- Step 9.1: Mutable Variables and Assignment
- Step 9.2: Stack Allocation with `alloca`
- Step 9.3: Automatic SSA Promotion with `mem2reg`

### Section 10: Compiling to Standalone Object Files
- Step 10.1: Target Machine Configuration
- Step 10.2: Emitting Relocatable `.o` Files
- Step 10.3: Linking with a Native C Driver
