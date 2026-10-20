# Step 5.1: Native Target Initialisation

Welcome to Step 5.1.  Before LLVM can generate executable machine code, it must initialise the code generator and assembly parser for your specific computer architecture (such as x86-64 or AArch64).

---

## The Target Select Helpers

LLVM supports dozens of target architectures, from embedded microcontrollers to supercomputers.  To avoid compiling support for every architecture into your executable, LLVM provides initialisation functions for the current host machine:

- `llvm::InitializeNativeTarget()`: Initialises the target machine code generator for the host CPU.
- `llvm::InitializeNativeTargetAsmPrinter()`: Initialises the target assembly and machine code emitter.
- `llvm::InitializeNativeTargetAsmParser()`: Initialises the target assembly parser.

---

## The Code Changes

Open `kaleidoscope.cpp` and include the target selection header:

```cpp
#include "llvm/Support/TargetSelect.h"
```

In `main()`, call the initialisation functions at the very top:

```cpp
int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  // (Remainder of your main function)
```

---

## Compiling and Running

Compile linking against `core` and `native`:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core native) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

The binary starts immediately, with the native code generator for your host architecture registered and ready for execution.

In [Step 5.2](02-jit-engine.md), we will introduce the JIT execution engine itself.
