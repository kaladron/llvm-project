# Kaleidoscope: Building a Compiler with LLVM

## The Philosophy of This Tutorial

Most compiler tutorials fail for a simple reason: they make you wait too long before anything actually runs.  They hand you hundreds of lines of disconnected code snippets, ask you to trust that it will all make sense later, and leave you staring at compiler errors when the pieces do not fit together.

This tutorial takes the opposite approach.  You will work in a single living file, building in tiny steps of five to fifteen lines of code.  Each step takes between one and three minutes to type, compiles immediately, and gives you instant visual feedback in your terminal.

Furthermore, genuine learning requires actively producing code rather than passively reading it.  Throughout this guide, we will progressively hand over responsibility to you: starting with clear scaffolding, and gradually guiding you to write functions and compiler extensions yourself.  Reference solutions and implementations are placed under collapsible folds so that you can stretch, experiment, and prove your own understanding before verifying your work.

By the end of this series, you will have written a complete, working programming language with an interactive REPL, an optimising JIT compiler, and an ahead-of-time code generator that produces standalone native object files.

---

## The Ground Rules: Active Learning

If your goal is simply to have a working Kaleidoscope binary on your machine, you can check out the LLVM source tree and build the existing examples.  The goal of this tutorial is different.  The goal is for you to understand compilers.

To get the most out of this tutorial, commit to two rules:

1.  **Type everything by hand.**  Do not copy and paste blocks of code from the browser into your editor.  Typing code forces your eyes to scan every token, helps build muscle memory for compiler data structures, and forces you to confront syntax errors when you make a typo.
2.  **No LLMs.**  Do not paste these chapters into a large language model to generate the code for you.  Using a code generator robs you of the specific friction that produces learning.  When you encounter a compiler error or an unexpected assertion failure, read the diagnostic, inspect the state of your AST or token stream, and track it down yourself.

---

## Choosing Your Learning Milestone (Where to Stop)

Not every developer needs to complete all sixteen sections.  You can reasonably stop at several natural milestones depending on what you wish to learn:

### Milestone 1: Frontends, Parsing, and ASTs (End of Section 3)
* **Target Audience**: Developers interested in lexical analysis, Abstract Syntax Tree representation, and recursive-descent parsing with operator precedence climbing, without touching machine code.
* **Outcome**: A self-contained, pure C++ AST-based calculator that evaluates mathematical expressions with correct operator precedence.

### Milestone 2: Interactive JIT Compilers (End of Section 5 or Section 8)
* **Target Audience**: Language designers interested in building interactive scripting engines, domain-specific languages (DSLs), or Just-In-Time execution engines.
* **Outcome**: By Section 5, you have an interactive REPL executing native machine code in memory using LLVM ORCv2.  By Section 8, you have added control flow, loops, and user-defined operators rendering the Mandelbrot fractal.

### Milestone 3: Classical Static Ahead-Of-Time Compilers (End of Section 10)
* **Target Audience**: Engineers building standalone compiled languages (like C, Go, or Rust) that emit relocatable `.o` object files and link with native C drivers into executables.
* **Outcome**: The complete Kaleidoscope 101 curriculum, compiling raw source text into independent executable binaries.

### Milestone 4: Systems Programming and Core LLVM (End of Section 16)
* **Target Audience**: Developers preparing to contribute to core LLVM or build production-grade systems languages.
* **Outcome**: The Kaleidoscope 201 curriculum, covering integer signedness, constant pools, opaque pointers (`ptr`), heap allocators, aggregate structs, `GetElementPtr` address math, DWARF debug information, and custom optimization passes.

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

---

## Part 2: Kaleidoscope 201 (Systems Programming & Core LLVM)

In Part 2, you transition from a single-type toy language to core compiler engineering.  You will implement multi-type semantics, memory layouts, DWARF debugging, and custom LLVM transformation passes.

### [Section 11: Primitive Types, Integers, and Casts](11-integers-and-types/index.md)
- [Step 11.1: Integer Widths and Primitive Types](11-integers-and-types/01-integer-widths.md) - Define explicit types and emit integer constants.
- [Step 11.2: Signless Registers and Integer Arithmetic](11-integers-and-types/02-signless-integers.md) - Generate integer math and understand the distinction between signed and unsigned operations.
- [Step 11.3: Integer Comparisons with 'icmp'](11-integers-and-types/03-integer-comparisons.md) - Compare integers with `icmp slt`, `icmp ult`, and equality checks.
- [Step 11.4: Type Conversions and Casts](11-integers-and-types/04-type-conversions.md) - Convert between integers and floating-point values with `sitofp`, `fptosi`, and bit truncation.

### [Section 12: Global Variables and Constant Strings](12-globals-and-strings/index.md)
- [Step 12.1: Emitting Global Variables](12-globals-and-strings/01-global-variables.md) - Allocate global variables in `.data` and `.bss`, and configure symbol linkage.
- [Step 12.2: String Literals and Constant Pools](12-globals-and-strings/02-string-literals.md) - Emit null-terminated string literals into `.rodata` with `CreateGlobalString`.
- [Step 12.3: Calling the C Standard Library](12-globals-and-strings/03-calling-clib.md) - Declare external functions with pointer arguments and call `puts` and `printf`.

### [Section 13: Pointers, Indirection, and Heap Memory](13-pointers-and-heap/index.md)
- [Step 13.1: Understanding LLVM Opaque Pointers](13-pointers-and-heap/01-opaque-pointers.md) - Learn how modern LLVM models memory addresses with `ptr`.
- [Step 13.2: Address-Of and Dereferencing](13-pointers-and-heap/02-address-and-dereference.md) - Implement C-style pointer operators `&` and `*` in LLVM IR.
- [Step 13.3: Heap Allocation with 'malloc' and 'free'](13-pointers-and-heap/03-heap-malloc-free.md) - Allocate and release dynamic heap buffers at runtime.

### [Section 14: Aggregate Structs, Arrays, and GetElementPtr](14-structs-and-gep/index.md)
- [Step 14.1: Struct Types and ABI DataLayout](14-structs-and-gep/01-struct-types-layout.md) - Define struct types, inspect memory sizing, and understand hardware padding.
- [Step 14.2: Demystifying GetElementPtr](14-structs-and-gep/02-understanding-gep.md) - Understand how GEP calculates pointer addresses without accessing memory.
- [Step 14.3: Arrays and Member Access in Practice](14-structs-and-gep/03-arrays-indexing.md) - Access struct fields and index array elements using GEP.

### [Section 15: Source-Level Debug Information (DWARF)](15-debug-info/index.md)
- [Step 15.1: Initialising DIBuilder and Compile Units](15-debug-info/01-dibuilder-setup.md) - Set up debug files, compile units, and DWARF standards.
- [Step 15.2: Subprograms and Instruction Locations](15-debug-info/02-subprograms-locations.md) - Attach line numbers and lexical scopes to generated instructions.
- [Step 15.3: Stepping Through Code with LLDB](15-debug-info/03-debugging-with-lldb.md) - Compile to object code and step through source lines in a real debugger.

### [Section 16: Writing a Custom LLVM Pass](16-custom-opt-pass/index.md)
- [Step 16.1: The Modern LLVM Pass Pipeline](16-custom-opt-pass/01-pass-pipeline.md) - Understand `PassInfoMixin`, analysis managers, and the new pass manager architecture.
- [Step 16.2: Inspecting and Modifying IR](16-custom-opt-pass/02-inspecting-modifying-ir.md) - Traverse basic blocks, match instructions, and eliminate redundant operations.
- [Step 16.3: Running Your Pass and Verifying Optimisations](16-custom-opt-pass/03-running-pass.md) - Register and execute your pass in a real optimization pipeline.
