# Kaleidoscope: Building a Compiler with LLVM

```{toctree}
:hidden:

01-lexer/index
02-parser-ast/index
03-operators-eval/index
04-llvm-ir/index
05-jit/index
06-control-flow/index
07-loops/index
08-user-defined-ops/index
09-local-variables/index
10-object-files/index
11-integers-and-types/index
12-globals-and-strings/index
13-pointers-and-heap/index
14-structs-and-gep/index
15-debug-info/index
16-custom-opt-pass/index
code/index
```

## The Philosophy of This Tutorial

Most compiler tutorials fail for a simple reason: they make you wait too long before anything actually runs.  They hand you hundreds of lines of disconnected code snippets, ask you to trust that it will all make sense later, and leave you staring at compiler errors when the pieces do not fit together.

This tutorial takes the opposite approach.  You will work in a single living file (`kaleidoscope.cpp`), building in small, focused steps.  Every single step compiles cleanly and gives you immediate, concrete feedback in your terminal.

Furthermore, genuine learning comes from actively producing code rather than passively reading it.  Throughout this guide, we will introduce a concept, walk through one concrete case together, and then ask you to stretch by implementing the parallel case yourself.  Full reference snapshots for every step live under collapsible folds (and in the [companion code index](code/index.md)) so you can try your hand first and verify your work right after.

By the end of this series, you will have evolved a tiny character scanner into a complete, statically typed systems language with an interactive REPL, an optimising ORCv2 JIT compiler, ahead-of-time native object emission, DWARF source-level debug information, and a custom LLVM optimisation pass.

---

## The Ground Rules: Active Learning

If your goal is simply to have a working Kaleidoscope binary on your machine, you can check out the LLVM source tree and build the examples directly.  The goal of this tutorial is different.  The goal is for you to understand how compilers and LLVM actually work.

To get the most out of this tutorial, commit to two rules:

1.  **Type everything by hand.**  Do not copy and paste blocks of code from the browser into your editor.  Typing code forces your eyes to scan every token, builds muscle memory for LLVM's C++ APIs, and teaches you how to read compiler errors when you make a typo.
2.  **No LLMs.**  Do not paste these chapters into a large language model to generate the code for you.  Using a code generator robs you of the exact friction that produces understanding.  When you hit a compiler error or an unexpected IR verifier failure, read the diagnostic, inspect your AST or basic blocks, and track it down yourself.

---

## Choosing Your Learning Milestone (Where to Stop)

Not every developer needs all sixteen sections on their first pass.  Because `kaleidoscope.cpp` compiles and runs at every step, you can pause cleanly at four natural milestones:

### Milestone 1: Frontends, Parsing, and ASTs (End of Section 3)
* **Target Audience**: Developers interested in lexical analysis, Abstract Syntax Tree design, and recursive-descent parsing with operator-precedence climbing, before introducing any LLVM dependencies.
* **Outcome**: A self-contained, pure C++17 AST calculator that parses and evaluates arithmetic expressions with proper operator precedence.

### Milestone 2: Interactive JIT Compilers (End of Section 5 or Section 8)
* **Target Audience**: Language designers building interactive scripting engines, domain-specific languages (DSLs), or Just-In-Time execution engines.
* **Outcome**: By Section 5, you have an interactive REPL compiling and running native machine code in memory via LLVM ORCv2.  By Section 8, you have added `if`/`then`/`else` control flow, `for` loops, and user-defined operators capable of rendering the Mandelbrot fractal in your terminal.

### Milestone 3: Classical Ahead-of-Time Compilers (End of Section 10)
* **Target Audience**: Engineers building standalone compiled languages that optimise SSA control flow with `mem2reg`, emit native `.o` object files, and link with C code into standalone executables.
* **Outcome**: The complete **Kaleidoscope 101** curriculum, taking source code all the way to native binaries on disk while preserving the interactive JIT REPL.

### Milestone 4: Systems Programming and Core LLVM (End of Section 16)
* **Target Audience**: Developers preparing to contribute to core LLVM or build statically typed systems languages like C, Rust, or Zig.
* **Outcome**: The **Kaleidoscope 201** curriculum, evolving your single living `kaleidoscope.cpp` compiler to support signed and unsigned integer widths, explicit casts, global variables, string literals and variadic `printf`, opaque pointers (`ptr<T>`), `&` and `*`, `malloc`/`free`, user-defined `struct` layouts, fixed-size arrays, `GetElementPtr` address calculation, DWARF source-level debugging in GDB and LLDB, and a custom New Pass Manager peephole pass.

---

## How the Kaleidoscope Language Evolves

Teaching lexing, parsing, SSA `phi` nodes, JIT compilation, and static type checking all at once is a recipe for cognitive overload.  Instead, Kaleidoscope grows in two deliberate phases inside the same `kaleidoscope.cpp` file.

### Part 1 (Sections 1 to 10): One Type, Full Compiler Pipeline

In Part 1, every value in Kaleidoscope is a 64-bit floating-point number (`double` in C++).  Keeping a single runtime type lets us build an entire end-to-end compiler without cluttering the AST with type annotations or type-checking rules:

```text
# Compute the x'th Fibonacci number.
def fib(x)
  if x < 3 then
    1
  else
    fib(x - 1) + fib(x - 2);

# Evaluate the 10th Fibonacci number in the JIT.
fib(10);
```

Even in Part 1, Kaleidoscope can declare and call external C library functions directly via `extern`:

```text
extern sin(x);
extern cos(x);

sin(0.5) * sin(0.5) + cos(0.5) * cos(0.5);
```

### Part 2 (Sections 11 to 16): Evolving into a Systems Language

Once your compiler can optimise SSA IR and emit both JIT machine code and native `.o` object files at the end of Section 10, Part 2 opens the hood on how LLVM models real systems languages.

Rather than throwing away your Part 1 compiler, you will evolve it smoothly.  Parameters and return types gain optional `: type` and `-> RetType` annotations (`int`, `uint`, `i64`, `byte`, `bool`, `void`, `ptr<T>`, `struct`, and fixed-size arrays), while unannotated parameters and functions continue to default to `double` so all of your Part 1 programs keep working:

```text
extern printf(fmt: ptr<byte>, ...) -> int;

struct Point {
  x: i64,
  y: i64
};

def sum_point(p: ptr<Point>) -> i64
  (*p).x + (*p).y;
```

By watching the same compiler grow from a floating-point calculator into a language with pointers, heap allocation, ABI-padded structs, DWARF debug info, and custom IR passes, you will see first-hand how every major piece of LLVM fits together.

---

## Prerequisites and Environment Check

Before writing code, verify that you have a working C++17 compiler and the LLVM development libraries and headers installed.

Run the following command in your terminal:

```bash
llvm-config --version --cxxflags --ldflags --system-libs --libs core
```

If `llvm-config` prints an LLVM version alongside compiler flags and library paths, your environment is ready.  We use `clang++` throughout this guide, and all reference snapshots are compiled with `-std=c++17 -fno-rtti -Wall -Wextra`.

---

## Tutorial Roadmap

### Part 1: Kaleidoscope 101 (Frontend, IR, JIT, and Object Emission)

#### [Section 1: The Lexer (From Raw Text to Tokens)](01-lexer/index.md)
* [Step 1.1: What is a Token?](01-lexer/01-first-token.md) - Understand tokens as atomic syntax units and build a single-character scanner.
* [Step 1.2: Skipping Whitespace](01-lexer/02-skipping-whitespace.md) - Teach the scanner to discard whitespace with single-character lookahead.
* [Step 1.3: Numeric Literals](01-lexer/03-numeric-literals.md) - Group multi-digit integers and floating-point decimals into numeric tokens.
* [Step 1.4: Identifiers and Keywords](01-lexer/04-identifiers-keywords.md) - Distinguish `def` and `extern` from variable and function names.
* [Step 1.5: Skipping Comments](01-lexer/05-skipping-comments.md) - Ignore `#` comments to the end of the line, plus an active learning challenge.

#### [Section 2: The Parser and AST (From Tokens to Trees)](02-parser-ast/index.md)
* [Step 2.1: The Base AST and Numbers](02-parser-ast/01-first-ast-node.md) - Define `ExprAST` and `NumberExprAST`, and wire up token buffering.
* [Step 2.2: Parentheses and Grouping](02-parser-ast/02-parentheses.md) - Group subexpressions with parentheses and recover cleanly from syntax errors.
* [Step 2.3: Variables](02-parser-ast/03-variables.md) - Represent and parse named variable references (`VariableExprAST`).
* [Step 2.4: Function Calls](02-parser-ast/04-function-calls.md) - Use one-token lookahead to distinguish variables from `CallExprAST` argument lists.

#### [Section 3: Operator Precedence and Evaluation](03-operators-eval/index.md)
* [Step 3.1: The Binary AST Node](03-operators-eval/01-binary-ast.md) - Define `BinaryExprAST` and see why naive left-to-right parsing misgroups `1 + 2 * 3`.
* [Step 3.2: The Precedence Table](03-operators-eval/02-precedence-table.md) - Assign numeric binding strengths to operators with `GetTokPrecedence()`.
* [Step 3.3: Precedence Climbing](03-operators-eval/03-precedence-climbing.md) - Implement recursive precedence climbing in `ParseBinOpRHS`.
* [Step 3.4: Immediate Evaluation](03-operators-eval/04-immediate-eval.md) - Walk the AST in pure C++ to turn your parser into an interactive calculator.

#### [Section 4: Generating LLVM Intermediate Representation](04-llvm-ir/index.md)
* [Step 4.1: LLVM Setup and Constant IR](04-llvm-ir/01-llvm-setup.md) - Initialise `LLVMContext`, `Module`, and `IRBuilder`, and emit `ConstantFP`.
* [Step 4.2: Binary Arithmetic in IR](04-llvm-ir/02-binary-ir.md) - Emit SSA floating-point instructions (`fadd`, `fsub`, `fmul`, `fdiv`, `fcmp`).
* [Step 4.3: Function Prototypes and Definitions](04-llvm-ir/03-functions-ir.md) - Parse `def` and `extern`, create basic blocks, and validate IR with `verifyFunction`.
* [Step 4.4: Function Calls and External Declarations](04-llvm-ir/04-calls-and-extern.md) - Emit `call` instructions to user functions and external C math routines.

#### [Section 5: The ORCv2 JIT (Executing Native Machine Code)](05-jit/index.md)
* [Step 5.1: Native Target Initialisation](05-jit/01-target-setup.md) - Register your host CPU's native target, assembler, and instruction printer.
* [Step 5.2: Starting the JIT Engine](05-jit/02-jit-engine.md) - Create `KaleidoscopeJIT` and configure each module's `DataLayout` and triple.
* [Step 5.3: Executing Expressions in Memory](05-jit/03-executing-expressions.md) - JIT-compile top-level expressions, execute them natively, and reclaim memory with `ResourceTracker`.
* [Step 5.4: Calling Functions Across Modules](05-jit/04-functions-and-externs.md) - Re-declare prototypes across per-expression modules and support live function redefinition.

#### [Section 6: Control Flow (`if` / `then` / `else`)](06-control-flow/index.md)
* [Step 6.1: Lexing and Parsing Conditionals](06-control-flow/01-if-ast.md) - Tokenise `if`, `then`, and `else` and construct `IfExprAST`.
* [Step 6.2: Basic Blocks and Conditional Branches](06-control-flow/02-basic-blocks-branching.md) - Split control flow across `then`, `else`, and `ifcont` basic blocks.
* [Step 6.3: Merging Values with SSA PHI Nodes](06-control-flow/03-phi-nodes.md) - Join values from divergent predecessors using `llvm::PHINode`.
* [Step 6.4: Recursive Fibonacci in the JIT](06-control-flow/04-fibonacci-jit.md) - Compile and execute recursive functions at native speed.

#### [Section 7: Loops and Iteration](07-loops/index.md)
* [Step 7.1: Lexing and Parsing `for` Loops](07-loops/01-for-ast.md) - Add `for`, `=`, and `in` syntax with optional step expressions.
* [Step 7.2: Loop Basic Blocks and Backedges](07-loops/02-loop-codegen.md) - Emit loop headers, induction `phi` nodes, variable shadowing, and exit branches.
* [Step 7.3: Terminal Graphics and Host C Functions](07-loops/03-terminal-graphics.md) - Export host C helpers (`putchard`, `printd`) with `-rdynamic` to draw ASCII art.

#### [Section 8: User-Defined Operators](08-user-defined-ops/index.md)
* [Step 8.1: Custom Operator Prototypes](08-user-defined-ops/01-custom-operators-ast.md) - Parse `unary` and `binary` operator prototypes with custom precedence levels.
* [Step 8.2: Prefix Unary Operators](08-user-defined-ops/02-unary-operators.md) - Parse prefix operators like `!v` and `-x` and lower them to `unary<op>` calls.
* [Step 8.3: User-Defined Binary Operators](08-user-defined-ops/03-binary-operators.md) - Register custom binary operators dynamically in `BinopPrecedence`.
* [Step 8.4: Rendering the Mandelbrot Set](08-user-defined-ops/04-mandelbrot.md) - Combine operators, loops, and conditionals to plot the Mandelbrot fractal in the JIT.

#### [Section 9: Mutable Local Variables and `mem2reg`](09-local-variables/index.md)
* [Step 9.1: Stack Allocation with `alloca`](09-local-variables/01-alloca-stack-variables.md) - Migrate `NamedValues` from `Value*` to entry-block `AllocaInst*` slots.
* [Step 9.2: Local `var`/`in` Bindings and Assignment](09-local-variables/02-var-in-expressions.md) - Add mutable local variable declarations and the `=` assignment operator.
* [Step 9.3: Automatic SSA Promotion with `mem2reg`](09-local-variables/03-mem2reg-optimisation.md) - Wire up LLVM's New Pass Manager and watch `PromotePass` rewrite stack slots into clean SSA registers.

#### [Section 10: Compiling to Standalone Object Files](10-object-files/index.md)
* [Step 10.1: Configuring `TargetMachine` and `--emit-obj`](10-object-files/01-target-machine.md) - Register all target backends and configure a PIC-ready `TargetMachine` alongside the JIT.
* [Step 10.2: Emitting Native `.o` Object Files](10-object-files/02-emitting-object-code.md) - Batch definitions into a single module and write native ELF/Mach-O/COFF `.o` files.
* [Step 10.3: Linking Standalone Executables](10-object-files/03-linking-standalone-binary.md) - Link Kaleidoscope `.o` files with a C driver (`main.c`) into a standalone native binary.

---

### Part 2: Kaleidoscope 201 (Systems Types, Memory, DWARF, and Passes)

#### [Section 11: Primitive Types, Integers, and Casts](11-integers-and-types/index.md)
* [Step 11.1: Integer Widths and Type Annotations](11-integers-and-types/01-integer-widths.md) - Introduce `KType` (`double`, `int`, `uint`, `i64`, `byte`, `bool`, `void`), `: type` annotations, and `-> RetType`.
* [Step 11.2: Signless Registers and Integer Arithmetic](11-integers-and-types/02-signless-integers.md) - Understand why LLVM `i32` is signless and emit `sdiv` vs `udiv`, `ashr` vs `lshr`, and bitwise ops.
* [Step 11.3: Integer Comparisons with `icmp`](11-integers-and-types/03-integer-comparisons.md) - Add `==`, `!=`, `<`, `<=`, `>`, and `>=` returning `i1` booleans directly into `CreateCondBr`.
* [Step 11.4: Explicit Type Conversions (`as`)](11-integers-and-types/04-type-conversions.md) - Implement `expr as <type>` with `sext`, `zext`, `trunc`, `sitofp`, `uitofp`, `fptosi`, and `fptoui`.

#### [Section 12: Global Variables and Constant Strings](12-globals-and-strings/index.md)
* [Step 12.1: Mutable Global Variables](12-globals-and-strings/01-global-variables.md) - Emit `GlobalVariable` definitions in `.data`/`.bss` and resolve them across JIT modules.
* [Step 12.2: String Literals in `.rodata`](12-globals-and-strings/02-string-literals.md) - Lex `"..."` escape sequences and emit constant string globals with `CreateGlobalString`.
* [Step 12.3: Variadic `extern` and Calling `printf`](12-globals-and-strings/03-calling-clib.md) - Parse `...` variadic prototypes, apply C default argument promotions, and call `puts` and `printf`.

#### [Section 13: Pointers, Indirection, and Heap Memory](13-pointers-and-heap/index.md)
* [Step 13.1: LLVM's Unified Opaque `ptr` Type](13-pointers-and-heap/01-opaque-pointers.md) - Parse `ptr<T>`, `ptr`, and `null`, and see how frontend pointee types lower to LLVM's unified `ptr`.
* [Step 13.2: Address-Of (`&`) and Dereference (`*`)](13-pointers-and-heap/02-address-and-dereference.md) - Reuse `codegenAddress()` for `&x` and support both `*p` loads and `*p = val` stores.
* [Step 13.3: `sizeof` and Heap Allocation (`malloc` / `free`)](13-pointers-and-heap/03-heap-malloc-free.md) - Query ABI byte sizes from `DataLayout` and manage dynamic heap buffers from the JIT.

#### [Section 14: Aggregate Structs, Arrays, and `GetElementPtr`](14-structs-and-gep/index.md)
* [Step 14.1: User-Defined `struct` Types and ABI Padding](14-structs-and-gep/01-struct-types-layout.md) - Parse `struct Name { ... };` and inspect live `DataLayout` size, alignment, and field byte offsets.
* [Step 14.2: Field Access and `GetElementPtr` (`CreateStructGEP`)](14-structs-and-gep/02-understanding-gep.md) - Demystify GEP address calculation and implement `.field` reads and writes.
* [Step 14.3: Fixed-Size Arrays and `base[i]` Indexing](14-structs-and-gep/03-arrays-indexing.md) - Add `[N x T]` array types and lower both array and pointer indexing with `CreateInBoundsGEP`.

#### [Section 15: Source-Level Debug Information (DWARF)](15-debug-info/index.md)
* [Step 15.1: `SourceLocation`, `-g`, and `DIBuilder` Setup](15-debug-info/01-dibuilder-setup.md) - Track line and column numbers in the lexer and emit `DICompileUnit` and DWARF module flags.
* [Step 15.2: Subprograms, Lexical Locations, and Variable Descriptors](15-debug-info/02-subprograms-locations.md) - Attach `DISubprogram`, `DILocation`, and `insertDeclare` variable metadata while keeping prologues clean.
* [Step 15.3: Source-Level Debugging with GDB and LLDB](15-debug-info/03-source-level-debugging.md) - Inspect DWARF DIEs with `llvm-dwarfdump` and step line-by-line through Kaleidoscope source in a native debugger.

#### [Section 16: Writing a Custom LLVM Optimisation Pass](16-custom-opt-pass/index.md)
* [Step 16.1: `PassInfoMixin` and `--run-peephole` Wiring](16-custom-opt-pass/01-pass-pipeline.md) - Add a custom `FunctionPass` skeleton and register `"ks-peephole"` with `PassBuilder`.
* [Step 16.2: Pattern Matching and Safe IR Rewriting](16-custom-opt-pass/02-inspecting-modifying-ir.md) - Match algebraic identities (`x + 0`, `x * 1`, `x - x`) with `llvm::PatternMatch` and rewrite SSA uses safely.
* [Step 16.3: Inspecting Before/After IR and Verifying Preservation](16-custom-opt-pass/03-running-pass.md) - Compare unoptimised and post-pass IR side by side, verify SSA invariants, and execute the optimised code in the JIT.

---

### [Companion Reference Code (`code/`)](code/index.md)
* Browse all 57 step snapshots (`s01-01-first-token.cpp` through `s16-03-running-pass.cpp`), plus `main.c`, `debug_main.c`, and phase-by-phase compilation commands.
