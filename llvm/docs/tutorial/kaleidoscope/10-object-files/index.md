# Section 10: Compiling to Standalone Object Files

Throughout Sections 5 through 9, you ran Kaleidoscope as an interactive Just-In-Time (JIT) compiler.  While JIT compilation is powerful for interactive exploration, languages intended for production software must be able to compile code **ahead-of-time (AOT)** into standard relocatable object files (`.o`).

In this final section, you will turn Kaleidoscope into a static ahead-of-time compiler.  You will configure LLVM's `TargetMachine`, emit standard object code formatted for your host operating system and CPU architecture, and link the resulting object file with a native C driver into a standalone executable.

---

## Steps in This Section

1.  [Step 10.1: Configuring the Target Machine](01-target-machine.md) - Initialise target registries, query the host target triple, and configure `llvm::TargetMachine`.
2.  [Step 10.2: Emitting Object Code](02-emitting-object-code.md) - Run LLVM's code emission passes to write a native `.o` object file to disk.
3.  [Step 10.3: Linking Standalone Executables](03-linking-standalone-binary.md) - Write a C driver and link with `clang` to produce a standalone executable.

---

## Compiling Your Compiler

Compiling an ahead-of-time code generator requires linking against LLVM's code generator and target libraries:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core target native codegen) -o kaleidoscope
```

Reference code implementations for each step are stored in the [code/](../code/) directory.
