# Step 4.2: Binary Math in IR

In this step, we generate LLVM instructions for mathematical operators (`+`, `-`, `*`, `/`, `<`).

---

## Floating-Point Instructions in LLVM

LLVM uses separate instructions for integer and floating-point arithmetic.  Because Kaleidoscope values are 64-bit doubles, we use LLVM floating-point builder methods:

| Operator | LLVM Instruction | IRBuilder Method |
| :--- | :--- | :--- |
| `+` | `fadd` | `Builder->CreateFAdd(L, R, "addtmp")` |
| `-` | `fsub` | `Builder->CreateFSub(L, R, "subtmp")` |
| `*` | `fmul` | `Builder->CreateFMul(L, R, "multmp")` |
| `/` | `fdiv` | `Builder->CreateFDiv(L, R, "divtmp")` |
| `<` | `fcmp ult` | `Builder->CreateFCmpULT(L, R, "cmptmp")` |

> [!NOTE]
> **Automatic Constant Folding in IRBuilder**:
> When you compile `1 + 2 * 3;`, the generated IR contains `ret double 7.000000e+00` rather than `fadd` and `fmul` instructions.  LLVM's `IRBuilder` checks whether incoming operands are constants upon insertion.  If both are constant, it computes the result at compile time and returns the folded constant directly, without needing an explicit optimization pass.

> [!WARNING]
> **Boolean Comparisons in Floating Point**:
> Comparison instructions like `CreateFCmpULT` return a 1-bit integer register (`i1`, representing 0 or 1).  Because Kaleidoscope treats every value as a 64-bit `double`, you cannot return an `i1` directly from a function expecting a double.  We must explicitly convert the `i1` back to a `double` (`0.0` or `1.0`) using `Builder->CreateUIToFP`.

---

## Active Authoring: Implementing BinaryExprAST::codegen()

In Step 3.4, you implemented `BinaryExprAST::eval()` using a switch statement over `Op`:

```cpp
double Left = LHS->eval();
double Right = RHS->eval();
switch (Op) {
case '+': return Left + Right;
...
```

Translating this to LLVM IR is a direct parallel:
* Evaluate operands to `llvm::Value *`:
  ```cpp
  llvm::Value *L = LHS->codegen();
  llvm::Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;
  ```
* For each arithmetic operator, invoke the corresponding builder method (e.g. `Builder->CreateFAdd(L, R, "addtmp")`).
* For `<`: perform `CreateFCmpULT(L, R, "cmptmp")` and convert the resulting `i1` to a `double` with `Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp")`.

Try implementing `BinaryExprAST::codegen()` directly in `kaleidoscope.cpp` before expanding the reference fold below.

<details>
<summary>Click here to reveal BinaryExprAST::codegen()</summary>

```cpp
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  llvm::Value *codegen() override {
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (!L || !R)
      return nullptr;

    switch (Op) {
    case '+':
      return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
      return Builder->CreateFSub(L, R, "subtmp");
    case '*':
      return Builder->CreateFMul(L, R, "multmp");
    case '/':
      return Builder->CreateFDiv(L, R, "divtmp");
    case '<':
      L = Builder->CreateFCmpULT(L, R, "cmptmp");
      return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext),
                                   "booltmp");
    default:
      fprintf(stderr, "invalid binary operator: %c\n", Op);
      return nullptr;
    }
  }
};
```
</details>

---

## Updating the Driver Loop

In your driver section, replace `HandleNumber()` with `HandleTopLevelExpression()` so your compiler can parse and generate code for compound arithmetic expressions:

```cpp
static void HandleTopLevelExpression() {
  if (auto AST = ParseExpression()) {
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
  } else {
    getNextToken();
  }
}
```

In `MainLoop()`, replace `case tok_number:` with a `default:` handler calling `HandleTopLevelExpression()`.

---

## Verifying Your Code

Before compiling, check that your `kaleidoscope.cpp` has all additions in place.

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

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
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

enum Token {
  tok_eof = -1,
  tok_number = -2,
};

static double NumVal;

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// LLVM State
//===----------------------------------------------------------------------===//

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

static void InitializeModule() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("Kaleidoscope", *TheContext);
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

//===----------------------------------------------------------------------===//
// AST
//===----------------------------------------------------------------------===//

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  llvm::Value *codegen() override {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
  }
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  llvm::Value *codegen() override {
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (!L || !R)
      return nullptr;

    switch (Op) {
    case '+':
      return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
      return Builder->CreateFSub(L, R, "subtmp");
    case '*':
      return Builder->CreateFMul(L, R, "multmp");
    case '/':
      return Builder->CreateFDiv(L, R, "divtmp");
    case '<':
      L = Builder->CreateFCmpULT(L, R, "cmptmp");
      return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext),
                                   "booltmp");
    default:
      fprintf(stderr, "invalid binary operator: %c\n", Op);
      return nullptr;
    }
  }
};

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[static_cast<char>(CurTok)];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken();
  return Result;
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  if (CurTok == tok_number)
    return ParseNumberExpr();
  fprintf(stderr, "Error: expected a number\n");
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken();

    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS = std::make_unique<BinaryExprAST>(static_cast<char>(BinOp),
                                          std::move(LHS), std::move(RHS));
  }
}

static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

//===----------------------------------------------------------------------===//
// Driver Loop
//===----------------------------------------------------------------------===//

static void HandleTopLevelExpression() {
  if (auto AST = ParseExpression()) {
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
  } else {
    getNextToken();
  }
}

static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      getNextToken();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

int main() {
  InitializeModule();

  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  printf("Kaleidoscope LLVM IR Generator (Step 4.2: Binary Math)\n");
  printf("Type math expressions followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  getNextToken();
  MainLoop();

  return 0;
}
```
</details>

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

Type a compound mathematical expression:

```text
Kaleidoscope LLVM IR Generator (Step 4.2: Binary Math)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
; ModuleID = 'Kaleidoscope'
source_filename = "Kaleidoscope"

define double @__anon_expr() {
entry:
  ret double 7.000000e+00
}
```

In [Step 4.3](03-functions-ir.md), we will introduce functions with parameters, where values are dynamic and full instructions are generated.
