# Companion Reference Code

Every step in the sixteen-section Kaleidoscope tutorial corresponds to a complete, self-contained C++17 source file in this `code/` directory.  As you work through your own `kaleidoscope.cpp`, you can compare your work against the exact snapshot for any step or compile a snapshot directly if you want to jump in at a later milestone.

:::{tip}
**Use Reference Files as a Safety Net**:
You will learn far more by typing each step into your own `kaleidoscope.cpp` and tracking down compiler diagnostics yourself.  Keep these files handy for checking subtle diffs or starting fresh at a new milestone.
:::

---

## Compilation Commands by Phase

Run these commands from inside `llvm/docs/tutorial/kaleidoscope/code/` (or adjust `-I../include` to `-Iinclude` when compiling from `llvm/docs/tutorial/kaleidoscope/`).

### Sections 1 to 3: Pure C++ Frontend and Evaluator

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra s03-04-immediate-eval.cpp -o kaleidoscope
```

### Section 4: LLVM IR Generation

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra s04-04-calls-and-extern.cpp \
  $(llvm-config --cxxflags --ldflags --system-libs --libs core) \
  -isystem $(llvm-config --includedir) -o kaleidoscope
```

### Sections 5 to 6: Interactive ORCv2 JIT

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -I../include s06-04-fibonacci-jit.cpp \
  $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) \
  -isystem $(llvm-config --includedir) -o kaleidoscope
```

### Sections 7 to 8: Host C Symbol Export (`-rdynamic`)

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -I../include s08-04-mandelbrot.cpp \
  $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) \
  -isystem $(llvm-config --includedir) -o kaleidoscope
```

### Section 9: Stack Allocas and the New Pass Manager

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -I../include s09-03-mem2reg-optimisation.cpp \
  $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes) \
  -isystem $(llvm-config --includedir) -o kaleidoscope
```

### Sections 10 to 16: Object Files, Systems Types, DWARF, and Custom Passes

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -I../include s16-03-running-pass.cpp \
  $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) \
  -isystem $(llvm-config --includedir) -o kaleidoscope
```

---

## Part 1: Kaleidoscope 101 Snapshots

### Section 1: The Lexer
* `s01-01-first-token.cpp` ([Step 1.1](../01-lexer/01-first-token.md))
* `s01-02-skipping-whitespace.cpp` ([Step 1.2](../01-lexer/02-skipping-whitespace.md))
* `s01-03-numeric-literals.cpp` ([Step 1.3](../01-lexer/03-numeric-literals.md))
* `s01-04-identifiers-keywords.cpp` ([Step 1.4](../01-lexer/04-identifiers-keywords.md))
* `s01-05-skipping-comments.cpp` ([Step 1.5](../01-lexer/05-skipping-comments.md))

### Section 2: The Parser and AST
* `s02-01-first-ast-node.cpp` ([Step 2.1](../02-parser-ast/01-first-ast-node.md))
* `s02-02-parentheses.cpp` ([Step 2.2](../02-parser-ast/02-parentheses.md))
* `s02-03-variables.cpp` ([Step 2.3](../02-parser-ast/03-variables.md))
* `s02-04-function-calls.cpp` ([Step 2.4](../02-parser-ast/04-function-calls.md))

### Section 3: Operator Precedence and Evaluation
* `s03-01-binary-ast.cpp` ([Step 3.1](../03-operators-eval/01-binary-ast.md))
* `s03-02-precedence-table.cpp` ([Step 3.2](../03-operators-eval/02-precedence-table.md))
* `s03-03-precedence-climbing.cpp` ([Step 3.3](../03-operators-eval/03-precedence-climbing.md))
* `s03-04-immediate-eval.cpp` ([Step 3.4](../03-operators-eval/04-immediate-eval.md))

### Section 4: Generating LLVM IR
* `s04-01-llvm-setup.cpp` ([Step 4.1](../04-llvm-ir/01-llvm-setup.md))
* `s04-02-binary-ir.cpp` ([Step 4.2](../04-llvm-ir/02-binary-ir.md))
* `s04-03-functions-ir.cpp` ([Step 4.3](../04-llvm-ir/03-functions-ir.md))
* `s04-04-calls-and-extern.cpp` ([Step 4.4](../04-llvm-ir/04-calls-and-extern.md))

### Section 5: The ORCv2 JIT
* `s05-01-target-setup.cpp` ([Step 5.1](../05-jit/01-target-setup.md))
* `s05-02-jit-engine.cpp` ([Step 5.2](../05-jit/02-jit-engine.md))
* `s05-03-executing-expressions.cpp` ([Step 5.3](../05-jit/03-executing-expressions.md))
* `s05-04-functions-and-externs.cpp` ([Step 5.4](../05-jit/04-functions-and-externs.md))

### Section 6: Control Flow (`if` / `then` / `else`)
* `s06-01-if-ast.cpp` ([Step 6.1](../06-control-flow/01-if-ast.md))
* `s06-02-basic-blocks-branching.cpp` ([Step 6.2](../06-control-flow/02-basic-blocks-branching.md))
* `s06-03-phi-nodes.cpp` ([Step 6.3](../06-control-flow/03-phi-nodes.md))
* `s06-04-fibonacci-jit.cpp` ([Step 6.4](../06-control-flow/04-fibonacci-jit.md))

### Section 7: Loops and Iteration
* `s07-01-for-ast.cpp` ([Step 7.1](../07-loops/01-for-ast.md))
* `s07-02-loop-codegen.cpp` ([Step 7.2](../07-loops/02-loop-codegen.md))
* `s07-03-terminal-graphics.cpp` ([Step 7.3](../07-loops/03-terminal-graphics.md))

### Section 8: User-Defined Operators
* `s08-01-custom-operators-ast.cpp` ([Step 8.1](../08-user-defined-ops/01-custom-operators-ast.md))
* `s08-02-unary-operators.cpp` ([Step 8.2](../08-user-defined-ops/02-unary-operators.md))
* `s08-03-binary-operators.cpp` ([Step 8.3](../08-user-defined-ops/03-binary-operators.md))
* `s08-04-mandelbrot.cpp` ([Step 8.4](../08-user-defined-ops/04-mandelbrot.md))

### Section 9: Local Variables and `mem2reg`
* `s09-01-alloca-stack-variables.cpp` ([Step 9.1](../09-local-variables/01-alloca-stack-variables.md))
* `s09-02-var-in-expressions.cpp` ([Step 9.2](../09-local-variables/02-var-in-expressions.md))
* `s09-03-mem2reg-optimisation.cpp` ([Step 9.3](../09-local-variables/03-mem2reg-optimisation.md))

### Section 10: Standalone Object Files
* `s10-01-target-machine.cpp` ([Step 10.1](../10-object-files/01-target-machine.md))
* `s10-02-emitting-object-code.cpp` ([Step 10.2](../10-object-files/02-emitting-object-code.md))
* `s10-03-linking-standalone-binary.cpp` ([Step 10.3](../10-object-files/03-linking-standalone-binary.md))
* `main.c` (C host driver linked against Kaleidoscope `.o` output in [Step 10.3](../10-object-files/03-linking-standalone-binary.md))

---

## Part 2: Kaleidoscope 201 Snapshots

### Section 11: Primitive Types, Integers, and Casts
* `s11-01-integer-widths.cpp` ([Step 11.1](../11-integers-and-types/01-integer-widths.md))
* `s11-02-signless-integers.cpp` ([Step 11.2](../11-integers-and-types/02-signless-integers.md))
* `s11-03-integer-comparisons.cpp` ([Step 11.3](../11-integers-and-types/03-integer-comparisons.md))
* `s11-04-type-conversions.cpp` ([Step 11.4](../11-integers-and-types/04-type-conversions.md))

### Section 12: Global Variables and Constant Strings
* `s12-01-global-variables.cpp` ([Step 12.1](../12-globals-and-strings/01-global-variables.md))
* `s12-02-string-literals.cpp` ([Step 12.2](../12-globals-and-strings/02-string-literals.md))
* `s12-03-calling-clib.cpp` ([Step 12.3](../12-globals-and-strings/03-calling-clib.md))

### Section 13: Pointers, Indirection, and Heap Memory
* `s13-01-opaque-pointers.cpp` ([Step 13.1](../13-pointers-and-heap/01-opaque-pointers.md))
* `s13-02-address-and-dereference.cpp` ([Step 13.2](../13-pointers-and-heap/02-address-and-dereference.md))
* `s13-03-heap-malloc-free.cpp` ([Step 13.3](../13-pointers-and-heap/03-heap-malloc-free.md))

### Section 14: Aggregate Structs, Arrays, and `GetElementPtr`
* `s14-01-struct-types-layout.cpp` ([Step 14.1](../14-structs-and-gep/01-struct-types-layout.md))
* `s14-02-understanding-gep.cpp` ([Step 14.2](../14-structs-and-gep/02-understanding-gep.md))
* `s14-03-arrays-indexing.cpp` ([Step 14.3](../14-structs-and-gep/03-arrays-indexing.md))

### Section 15: Source-Level Debug Information (DWARF)
* `s15-01-dibuilder-setup.cpp` ([Step 15.1](../15-debug-info/01-dibuilder-setup.md))
* `s15-02-subprograms-locations.cpp` ([Step 15.2](../15-debug-info/02-subprograms-locations.md))
* `s15-03-source-level-debugging.cpp` ([Step 15.3](../15-debug-info/03-source-level-debugging.md))
* `debug_main.c` (C host driver for stepping through Kaleidoscope code in GDB or LLDB in [Step 15.3](../15-debug-info/03-source-level-debugging.md))

### Section 16: Writing a Custom LLVM Pass
* `s16-01-pass-pipeline.cpp` ([Step 16.1](../16-custom-opt-pass/01-pass-pipeline.md))
* `s16-02-inspecting-modifying-ir.cpp` ([Step 16.2](../16-custom-opt-pass/02-inspecting-modifying-ir.md))
* `s16-03-running-pass.cpp` ([Step 16.3](../16-custom-opt-pass/03-running-pass.md))

---

[Back to Tutorial Overview](../index.md)
