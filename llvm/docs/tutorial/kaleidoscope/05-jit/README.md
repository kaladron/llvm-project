# Section 5: The JIT (Executing Native Machine Code)

In [Section 4](../04-llvm-ir/README.md), you generated LLVM Intermediate Representation (IR) from your Kaleidoscope code.  However, printing text IR is not the same as executing code.

In this section, you will turn Kaleidoscope into a true **Just-In-Time (JIT) compiler**.  Using LLVM's modern On-Request Compilation (ORCv2) infrastructure, your compiler will take in-memory LLVM IR, compile it directly into native machine instructions for your host CPU, and execute those instructions on the bare metal.

You will continue extending `kaleidoscope.cpp` in four bite-sized steps.  By the end of this section, you will type `4 + 5;` and see `Evaluated to 9.000000` computed by machine code running natively on your computer.

---

## Steps in This Section

1.  [Step 5.1: Native Target Initialisation](01-target-setup.md) - Initialise the target machine for your host processor architecture.
2.  [Step 5.2: The JIT Engine](02-jit-engine.md) - Set up the `KaleidoscopeJIT` instance and configure the module data layout.
3.  [Step 5.3: Executing Expressions](03-executing-expressions.md) - Compile and execute top-level mathematical expressions natively in memory.
4.  [Step 5.4: Functions and Library Calls](04-functions-and-externs.md) - Call user-defined functions and external C standard library functions from the JIT REPL, plus an active learning challenge.

---

## Compiling with JIT Libraries

In this section, our compiler links against both the LLVM core libraries and the ORC JIT and native target components:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Full reference implementations for each step are stored in the [code/](../code/) directory.
