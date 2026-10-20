# Step 5.2: The JIT Engine

In this step, we introduce `KaleidoscopeJIT`, an On-Request Compilation (ORCv2) JIT class designed for this tutorial.

---

## What the JIT Does

The JIT compiler manages in-memory compilation and dynamic linking:
1.  **Module compilation**: When you add an LLVM IR module to the JIT via `addModule()`, the JIT compiles the IR into relocatable machine code in memory.
2.  **Symbol lookup**: When you call `lookup("my_func")`, the JIT resolves the in-memory memory address of the compiled function.
3.  **Process symbol resolution**: If a function was declared as `extern` (like `sin` or `cos`), the JIT automatically searches the symbols of the running host process to find the standard C library function.

The complete JIT class definition is provided in [include/KaleidoscopeJIT.h](../include/KaleidoscopeJIT.h).

---

## The Code Changes

Open `kaleidoscope.cpp` and include `KaleidoscopeJIT.h`:

```cpp
#include "KaleidoscopeJIT.h"
```

Add the global JIT pointer and error handler:

```cpp
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static llvm::ExitOnError ExitOnErr;
```

Update `InitializeModule()` to also set the module's data layout from the JIT:

```cpp
static void InitializeModuleAndManagers() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("KaleidoscopeJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}
```

In `main()`, instantiate the JIT before initialising the module:

```cpp
  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  InitializeModuleAndManagers();
```

---

## Compiling and Running

Compile linking against `core`, `orcjit`, and `native`:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

The JIT execution session starts cleanly and initialises the target data layout.

In [Step 5.3](03-executing-expressions.md), we will execute anonymous top-level expressions and print their results live in your terminal.
