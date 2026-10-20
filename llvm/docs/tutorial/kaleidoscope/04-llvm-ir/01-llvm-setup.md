# Step 4.1: LLVM Setup and Constant IR

Welcome to Step 4.1.  In this step, we connect our code to the LLVM infrastructure and generate our very first LLVM IR instruction: a floating-point constant.

---

## Core LLVM Data Structures

To generate LLVM IR, we use three primary objects:

1.  `llvm::LLVMContext`: An opaque container that owns core LLVM data structures, such as constant tables and type caches.
2.  `llvm::Module`: The top-level container for functions and global variables.  In LLVM, a module represents a single translation unit.
3.  `llvm::IRBuilder<>`: A helper class that makes it easy to generate LLVM instructions and append them to basic blocks.

---

## The Code Changes

Open `kaleidoscope.cpp` and include the LLVM headers at the top:

```cpp
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
```

Add the global LLVM state and an initialisation helper above the AST classes:

```cpp
static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

static void InitializeModule() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("Kaleidoscope", *TheContext);
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}
```

Add a virtual `codegen()` method to `ExprAST`:

```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
};
```

Implement `NumberExprAST::codegen()`.  In LLVM, constants are represented by `ConstantFP` using an `APFloat` (Arbitrary Precision Float):

```cpp
class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  llvm::Value *codegen() override {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
  }
};
```

In your driver loop, wrap expressions in an anonymous function and print the module to standard error:

```cpp
static void HandleNumber() {
  if (auto AST = ParseNumberExpr()) {
    auto *FnType =
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), false);
    auto *Fn = llvm::Function::Create(FnType, llvm::Function::ExternalLinkage,
                                      "__anon_expr", TheModule.get());
    auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", Fn);
    Builder->SetInsertPoint(BB);

    if (auto *RetVal = AST->codegen()) {
      Builder->CreateRet(RetVal);
      TheModule->print(llvm::errs(), nullptr);
    }

    Fn->eraseFromParent();
  }
}
```

Call `InitializeModule()` in `main()` before starting the REPL.

---

## Compiling and Running

Compile linking against LLVM core:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type a number:

```text
Kaleidoscope LLVM IR Generator (Step 4.1: Numbers)
Type a number followed by ';' and press Enter (Ctrl+D to exit):

ready> 42;
; ModuleID = 'Kaleidoscope'
source_filename = "Kaleidoscope"

define double @__anon_expr() {
entry:
  ret double 4.200000e+01
}
```

You just generated real, valid LLVM Intermediate Representation from your own AST.

In [Step 4.2](02-binary-ir.md), we will generate floating-point math instructions for addition, subtraction, multiplication, and division.
