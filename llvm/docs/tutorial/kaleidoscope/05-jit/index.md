# Section 5: The JIT (Executing Native Machine Code)

```{toctree}
:hidden:

01-target-setup
02-jit-engine
03-executing-expressions
04-functions-and-externs
```

In [Section 4](../04-llvm-ir/index.md), your compiler learned to translate Kaleidoscope expressions, functions, and `extern` declarations into validated LLVM Intermediate Representation (IR).  Printing text IR proves our frontend works, though nothing beats watching your CPU execute that code natively!

In this section, you will turn Kaleidoscope into a true **Just-In-Time (JIT) compiler**.  Using LLVM's On-Request Compilation (**ORCv2**) framework, your REPL will compile in-memory LLVM IR straight into native machine instructions for your host processor, link external C math symbols like `sin` and `cos` dynamically, and call your compiled functions at full hardware speed.

You will continue working directly inside `kaleidoscope.cpp` across four steps, finishing **Milestone 2** with live interactive function redefinition!

---

## Steps in This Section

1.  [Step 5.1: Native Target Initialisation](01-target-setup.md) - Register LLVM's native code generator and assembly printer for your host CPU and inspect your target triple.
2.  [Step 5.2: The JIT Engine](02-jit-engine.md) - Grab `include/KaleidoscopeJIT.h`, instantiate `KaleidoscopeJIT`, and configure your module's `DataLayout`.
3.  [Step 5.3: Executing Expressions](03-executing-expressions.md) - Compile `@__anon_expr` to native machine code, handle missing-symbol errors gracefully, and reclaim memory with `ResourceTracker`.
4.  [Step 5.4: Functions and `extern` Calls in the JIT](04-functions-and-externs.md) - Cache prototypes across modules with `FunctionProtos`, support live function redefinition via `FunctionRTs`, and call `libm` functions natively.

---

## Compiling Against ORC JIT and Native Target Libraries

Throughout Section 5, place `KaleidoscopeJIT.h` in an `include/` folder next to your `kaleidoscope.cpp` (we walk through grabbing it in [Step 5.2](02-jit-engine.md)) and link against LLVM's `core`, `orcjit`, and `native` components:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Collapsible reference snapshots appear inside every step, and standalone files are stored in the [Reference Code Archive](../code/index.md).

---

[Previous Section: Generating LLVM IR](../04-llvm-ir/index.md) | [Tutorial Overview](../index.md) | [Start: Step 5.1 - Native Target Initialisation](01-target-setup.md) | [Next Section: Control Flow](../06-control-flow/index.md)
