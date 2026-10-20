# Step 10.2: Emitting Object Code

Once `TargetMachine` is configured, emitting an object file requires only a few lines of code.

---

## 1. Opening the Output File

Create an output stream writing to a binary file named `output.o`:

```cpp
  std::error_code EC;
  llvm::raw_fd_ostream dest("output.o", EC, llvm::sys::fs::OF_None);
  if (EC) {
    llvm::errs() << "Could not open file: " << EC.message();
    return 1;
  }
```

---

## 2. Running the Object Emission Pass

In LLVM, converting Intermediate Representation into machine instructions (instruction selection, register allocation, and ELF/Mach-O/COFF emission) is orchestrated by a code generation pass pipeline.

Use `TargetMachine::addPassesToEmitFile` to configure the pass manager for object emission, run it on your module, and flush the file stream:

```cpp
  llvm::legacy::PassManager pass;
  auto FileType = llvm::CodeGenFileType::ObjectFile;

  if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    llvm::errs() << "TheTargetMachine can't emit a file of this type";
    return 1;
  }

  pass.run(*TheModule);
  dest.flush();

  fprintf(stderr, "Wrote output.o successfully!\n");
```

---

## 3. Compile and Run

Compile your compiler:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core target native codegen) -o kaleidoscope
```

Now run `./kaleidoscope` and enter a function definition:

```
ready> def average(x y) (x + y) * 0.5;
Parsed a function definition.
```

Press `Ctrl+D` to end input.  Your compiler runs the object emission pipeline:

```
Wrote output.o successfully!
```

Check the generated file:

```bash
file output.o
```

On Linux x86-64, the system reports:
```
output.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
```

You have generated a real, relocatable object file containing machine code for your host processor.

---

Next: [Step 10.3: Linking Standalone Executables](03-linking-standalone-binary.md) - Link `output.o` with C code into a standalone binary.
