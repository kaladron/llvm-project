# Section 10: Compiling to Standalone Object Files

```{toctree}
:hidden:

01-target-machine
02-emitting-object-code
03-linking-standalone-binary
```

Across Sections 5 through 9, you built an interactive Just-In-Time (JIT) compiler powered by LLVM ORCv2 and the New Pass Manager.  Interactive JIT execution is unbeatable for rapid prototyping, REPL exploration, and dynamic workloads, yet production languages also need to compile source code **Ahead-Of-Time (AOT)** into native relocatable object files (`.o`) that ship without any compiler runtime attached.

Better yet, you do not have to tear out your JIT to build an AOT compiler!  In this section, you will add a `--emit-obj <output.o>` command-line flag directly on top of your complete [Step 9.3](../09-local-variables/03-mem2reg-optimisation.md) compiler:

- Run `./kaleidoscope` with no flags, and you get the full interactive optimising JIT REPL.
- Run `./kaleidoscope --emit-obj output.o < math.ks`, and your compiler batches every definition into a single `llvm::Module`, optimises each function with `mem2reg` and scalar passes, and writes a native ELF, Mach-O, or COFF `.o` object file ready to link with `clang`!

---

## Steps in This Section

1.  [Step 10.1: Configuring the Target Machine](01-target-machine.md) - Parse `--emit-obj`, initialise LLVM's target registries, construct `llvm::TargetMachine` with Position-Independent Code (`Reloc::PIC_`), and batch definitions into a single module.
2.  [Step 10.2: Emitting Object Code](02-emitting-object-code.md) - Wire up `TargetMachine::addPassesToEmitFile` to translate optimised LLVM IR into native machine code on disk.
3.  [Step 10.3: Linking Standalone Executables](03-linking-standalone-binary.md) - Inspect symbols with `nm`, link `output.o` against a C `main.c` driver into a standalone binary, and celebrate completing Part 1!

---

## Compiling Your Compiler

Add `all-targets` to `--libs` so `llvm-config` links all configured target backends, MC object writers, and assembly printers alongside your existing JIT and pass libraries:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference implementations for every step are available in [code/index.md](../code/index.md).

---

[Previous: Section 9 - Local Variables and Mutability](../09-local-variables/index.md) | [Tutorial Home](../index.md) | [Next: Step 10.1 - Configuring the Target Machine](01-target-machine.md)
