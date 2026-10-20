# Step 4.3: Function Definitions

In this step, we add support for defining functions like `def square(x) x * x;`.

---

## Function Prototypes and Function Bodies

A function definition in Kaleidoscope consists of two pieces:
1.  **Prototype**: The function name and argument names (`PrototypeAST`).  In LLVM, this corresponds to an `llvm::Function` signature.
2.  **Body**: An expression that implements the function (`FunctionAST`).  In LLVM, this generates basic blocks and instructions ending with a return instruction (`ret`).

We also maintain a symbol table, `std::map<std::string, llvm::Value *> NamedValues`, which maps argument names to their LLVM parameter values.

---

## The Code Changes

Add the `PrototypeAST` and `FunctionAST` classes to `kaleidoscope.cpp`:

```cpp
#include "llvm/IR/Verifier.h"

static std::map<std::string, llvm::Value *> NamedValues;

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}

  llvm::Value *codegen() override {
    llvm::Value *V = NamedValues[Name];
    if (!V) {
      fprintf(stderr, "Error: unknown variable name: %s\n", Name.c_str());
      return nullptr;
    }
    return V;
  }
};

class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(std::string Name, std::vector<std::string> Args)
      : Name(std::move(Name)), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
  llvm::Function *codegen();
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  llvm::Function *codegen();
};
```

Implement `codegen()` for prototypes and functions:

```cpp
llvm::Function *PrototypeAST::codegen() {
  std::vector<llvm::Type *> Doubles(Args.size(),
                                    llvm::Type::getDoubleTy(*TheContext));
  auto *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext),
                                     Doubles, false);
  auto *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name,
                                   TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  return F;
}

llvm::Function *FunctionAST::codegen() {
  auto *TheFunction = TheModule->getFunction(Proto->getName());
  if (!TheFunction)
    TheFunction = Proto->codegen();
  if (!TheFunction)
    return nullptr;

  auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;

  if (auto *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    llvm::verifyFunction(*TheFunction);
    return TheFunction;
  }

  TheFunction->eraseFromParent();
  return nullptr;
}
```

Notice the call to `llvm::verifyFunction(*TheFunction)`: this performs consistency checks on the generated function to verify that all instructions are valid SSA form.

---

## Compiling and Running

Compile:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Define a function:

```text
Kaleidoscope LLVM IR Generator (Step 4.3: Functions)
Type function definitions or expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> def square(x) x * x;
Read function definition:
define double @square(double %x) {
entry:
  %multmp = fmul double %x, %x
  ret double %multmp
}
```

Because `x` is a runtime function parameter, `IRBuilder` cannot fold it away at compile time.  It generates a real `%multmp = fmul double %x, %x` instruction and returns it.

In [Step 4.4](04-calls-and-extern.md), we will add function calls and external declarations with `extern`.
