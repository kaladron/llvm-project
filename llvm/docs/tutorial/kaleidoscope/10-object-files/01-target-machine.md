# Step 10.1: Configuring the Target Machine

To emit machine code for a specific CPU, LLVM must know the details of the host architecture: register conventions, pointer widths, ABI calling conventions, and instruction set extensions.  All of this configuration is managed through **`llvm::TargetMachine`**.

---

## 1. Initialising Target Registries

Include the target registry and host parsing headers in `kaleidoscope.cpp`:

```cpp
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
```

In `main()`, initialise all supported LLVM target info, assembly parsers, and machine code printers:

```cpp
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();
```

---

## 2. Setting the Target Triple

A **Target Triple** identifies the target platform using the convention `[architecture]-[vendor]-[operating system]-[environment]`.  For example: `x86_64-unknown-linux-gnu` or `arm64-apple-darwin`.

Query LLVM for your current host triple and set it on the module:

```cpp
  llvm::Triple TargetTriple(llvm::sys::getDefaultTargetTriple());
  TheModule->setTargetTriple(TargetTriple);
```

---

## 3. Creating the `TargetMachine`

Look up the target architecture in LLVM's registry:

```cpp
  std::string Error;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
  if (!Target) {
    llvm::errs() << Error;
    return 1;
  }
```

Now construct the `TargetMachine` using Position-Independent Code (`PIC_`):

```cpp
  auto CPU = "generic";
  auto Features = "";
  llvm::TargetOptions opt;
  auto TheTargetMachine = Target->createTargetMachine(
      TargetTriple, CPU, Features, opt, llvm::Reloc::PIC_);
```

Finally, configure your module's data layout to match the target machine:

```cpp
  TheModule->setDataLayout(TheTargetMachine->createDataLayout());
```

---

Next: [Step 10.2: Emitting Object Code](02-emitting-object-code.md) - Run LLVM passes to write a native `.o` file.
