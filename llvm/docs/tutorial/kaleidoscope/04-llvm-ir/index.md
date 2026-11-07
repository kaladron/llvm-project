# Section 4: Generating LLVM Intermediate Representation

```{toctree}
:hidden:

01-llvm-setup
02-binary-ir
03-functions-ir
04-calls-and-extern
```

In [Section 3](../03-operators-eval/index.md), you built an Abstract Syntax Tree with operator-precedence climbing and evaluated expressions directly in pure C++.

Now we connect our frontend to the **LLVM compiler framework**.  In this section, you will translate your AST into **LLVM Intermediate Representation (IR)**: a strongly typed, target-independent instruction set in Static Single Assignment (SSA) form that LLVM uses to optimise programs and emit native machine code for x86-64, AArch64, RISC-V, WebAssembly, and beyond.

You will continue working directly inside your single `kaleidoscope.cpp` file across four focused steps, inspecting the exact LLVM IR your compiler emits at every stage.

---

## Steps in This Section

1.  [Step 4.1: LLVM Setup and Constant IR](01-llvm-setup.md) - Initialise `LLVMContext`, `Module`, and `IRBuilder`, replace `eval()` with `codegen()`, and emit IR for numeric constants.
2.  [Step 4.2: Binary Math in IR](02-binary-ir.md) - Emit `fadd`, `fsub`, `fmul`, `fdiv`, and `fcmp ult`, and watch `IRBuilder` constant-fold pure literal expressions automatically.
3.  [Step 4.3: Function Definitions](03-functions-ir.md) - Parse `def` functions, bind parameters in `NamedValues`, and validate generated IR with `llvm::verifyFunction`.
4.  [Step 4.4: Function Calls and `extern`](04-calls-and-extern.md) - Emit `call` instructions, declare external C math functions with `extern`, and experiment with multi-argument IR.

---

## Compiling Against LLVM Core Libraries

Starting in this section, your compiler includes LLVM headers and links against LLVM's core IR libraries.  Use `llvm-config` to supply the exact C++ flags, include path, and library flags for your machine:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -isystem $(llvm-config --includedir) -o kaleidoscope
```

:::{tip}
Why pass `-isystem $(llvm-config --includedir)` alongside `$(llvm-config --cxxflags)`?  Marking LLVM's header directory as a system include path silences `-Wall -Wextra` warnings inside external LLVM headers while keeping strict warnings active on every line of *your* `kaleidoscope.cpp`!
:::

Collapsible reference snapshots appear inside every step, and standalone files are stored in the [Reference Code Archive](../code/index.md).

---

[Previous Section: Operator Precedence and Evaluation](../03-operators-eval/index.md) | [Tutorial Overview](../index.md) | [Start: Step 4.1 - LLVM Setup and Constant IR](01-llvm-setup.md) | [Next Section: The JIT](../05-jit/index.md)
