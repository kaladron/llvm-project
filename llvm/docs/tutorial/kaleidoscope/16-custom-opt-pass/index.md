# Section 16: Writing a Custom LLVM Pass

Up until now, you have used LLVM as a code generator: taking an AST and emitting instructions.  However, much of the true power of LLVM lies in its **transformation and optimisation infrastructure**.

In core LLVM development, engineers constantly write and refine **passes**: modular units of compiler logic that analyse or transform Intermediate Representation to make code smaller, faster, or safer.

In this final capstone section of Kaleidoscope 201, you will learn how to write a real LLVM optimization pass using the modern **PassManager** infrastructure.  You will inspect instructions, detect dead arithmetic patterns like $x + 0$, replace redundant computations, and safely delete dead code.

---

## Steps in This Section

1.  [Step 16.1: The Modern LLVM Pass Pipeline](01-pass-pipeline.md) - Understand `PassInfoMixin`, analysis managers, and the new pass manager architecture.
2.  [Step 16.2: Inspecting and Modifying IR](02-inspecting-modifying-ir.md) - Traverse basic blocks, match instructions, and eliminate redundant operations.
3.  [Step 16.3: Running Your Pass and Verifying Optimisations](03-running-pass.md) - Register and execute your pass in a real optimization pipeline.

---

## Compiling Your Compiler

Compile against LLVM core and passes libraries:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core passes) -o kaleidoscope
```

Reference code implementations for each step are stored in the [code/](../code/) directory.
