# Section 15: Source-Level Debug Information (DWARF)

```{toctree}
:hidden:

01-dibuilder-setup
02-subprograms-locations
03-source-level-debugging
```

When a compiled program misbehaves, systems programmers do not stare at raw hex bytes in a disassembly dump.  They launch a source-level debugger like **`gdb`** or **`lldb`**, set a breakpoint on a source file and line number, step line by line, and print local variables by name.

How does a native debugger know that machine address `0x118c` corresponds to line 4 of `demo_debug.ks`, or that `RSP - 12` holds a 32-bit signed integer named `acc`?  The answer is **DWARF Debug Information**.

In this section, you will add a `-g` flag and source-file input mode to `kaleidoscope.cpp`, use LLVM's **`DIBuilder`** to emit DWARF compile units, type descriptors, subprograms, local variable declarations, and instruction line locations, and step live through a compiled Kaleidoscope function in GDB and LLDB.

---

## DWARF Metadata Architecture in LLVM IR

```text
  DICompileUnit (File: "demo_debug.ks", Producer: "Kaleidoscope Compiler")
    |
    +-- DISubprogram ("factorial", line 1, type: int(int))
          |
          +-- DILocalVariable ("n", arg 1)  ---> #dbg_declare(ptr %n1, ...)
          +-- DILocalVariable ("acc")       ---> #dbg_declare(ptr %acc, ...)
          +-- DILocation (line 4, col 18)   ---> attached via !dbg to IR instructions
                                                       |
                                                       v
                              TargetMachine lowers IR + metadata into ELF sections:
                                .debug_info   (DIE tree: functions, params, locals, types)
                                .debug_line   (PC address <--> file:line:col matrix)
```

:::{note}
**Separation of Executable IR and Debug Metadata**:
In LLVM IR, debug information lives in auxiliary metadata nodes (`!dbg` attachments and `#dbg_declare` records) rather than executable machine instructions.  Generating `-g` metadata never changes the runtime semantics of your program, and disabling aggressive register promotion (`mem2reg`) when `-g` is active keeps every parameter and local variable in a predictable stack slot for effortless single-stepping.
:::

---

## Steps in This Section

1.  [Step 15.1: Initialising DIBuilder and Compile Units](01-dibuilder-setup.md) - Track line and column numbers in the lexer, add `-g` and `.ks` file input, map `KType` to `DIType`, and configure DWARF module flags.
2.  [Step 15.2: Subprograms, Variables, and Source Locations](02-subprograms-locations.md) - Emit `DISubprogram` scopes, location-less function prologues, parameter and local `#dbg_declare` records, and instruction `DILocation` tags.
3.  [Step 15.3: Source-Level Debugging with GDB and LLDB](03-source-level-debugging.md) - Compile `demo_debug.ks` with `-g --emit-obj`, inspect DWARF DIEs with `llvm-dwarfdump`, and step through `factorial(5)` in a native debugger.

---

## Compiling Your Compiler

Compile `kaleidoscope.cpp` with the standard Part 2 build command:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference snapshots for each step are indexed in [code/index.md](../code/index.md).

---

[Previous: Fixed-Size Arrays and Subscript Indexing](../14-structs-and-gep/03-arrays-indexing.md) | [Tutorial Overview](../index.md) | [Start Step 15.1](01-dibuilder-setup.md)
