# Step 9.3: Automatic SSA Promotion with 'mem2reg'

Generating memory loads and stores with `alloca` solved the frontend problem of mutable variables.  However, loading and storing to memory on every variable access is slow.  Modern CPUs prefer values stored directly in machine registers.

In LLVM, you do not need to handcraft complex SSA algorithms to fix this.  LLVM provides the **`mem2reg`** optimisation pass.

`mem2reg` inspects stack allocations in the entry basic block.  If their addresses are never leaked (e.g. via pointers), it lifts those stack variables directly into SSA virtual registers and constructs PHI nodes where branches merge.

---

## 1. Adding `mem2reg` to the Function Pass Pipeline

Include the pass builder and `mem2reg` headers in `kaleidoscope.cpp`:

```cpp
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
```

In `FunctionAST::codegen()`, right before returning `TheFunction`, run the `PromotePass`:

```cpp
  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    llvm::verifyFunction(*TheFunction);

    // Optimise with mem2reg!
    llvm::FunctionAnalysisManager FAM;
    llvm::PassBuilder PB;
    PB.registerFunctionAnalyses(FAM);
    llvm::FunctionPassManager FPM;
    FPM.addPass(llvm::PromotePass());
    FPM.run(*TheFunction, FAM);

    return TheFunction;
  }
```

---

## 2. Compile and Observe the IR

Compile your compiler, linking the `passes` component:

```bash
clang++ -rdynamic -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes) -o kaleidoscope
```

Launch `./kaleidoscope` and define a function using mutable variables:

```
ready> def testvar(x)
         var a = 1, b = 2 in
           a = a + x;
Parsed a function definition.
define double @testvar(double %x) {
entry:
  %addtmp = fadd double 1.000000e+00, %x
  ret double %addtmp
}
```

Look at the emitted IR:
Every single `alloca`, `store`, and `load` instruction has completely vanished!  LLVM analysed the lifetimes of the variables, eliminated the memory allocations, and folded the operations into a single SSA floating-point addition (`fadd`).

Test evaluation:

```
ready> testvar(10);
Evaluated to 11.000000
```

---

Next: [Section 10: Compiling to Standalone Object Files](../10-object-files/index.md) - Turn Kaleidoscope into an ahead-of-time (AOT) static compiler.
