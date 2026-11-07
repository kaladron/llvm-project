# Step 5.2: The JIT Engine

In [Step 5.1](01-target-setup.md), we registered the native backend for our host CPU.  Now we instantiate **`KaleidoscopeJIT`**, a compact wrapper around LLVM's ORCv2 (On-Request Compilation) APIs, and align our `llvm::Module` with the JIT's target `DataLayout`.

---

## Grabbing `include/KaleidoscopeJIT.h`

Rather than pasting a hundred lines of ORCv2 boilerplate into `kaleidoscope.cpp`, this tutorial ships a clean, single-header JIT wrapper in [KaleidoscopeJIT.h](../include/KaleidoscopeJIT.h) (matching `llvm/examples/Kaleidoscope/include/KaleidoscopeJIT.h` in the LLVM source tree).

Before editing `kaleidoscope.cpp`, make sure `KaleidoscopeJIT.h` sits inside an `include/` directory next to your `kaleidoscope.cpp`:

```text
your-working-dir/
  +-- kaleidoscope.cpp
  +-- include/
        +-- KaleidoscopeJIT.h
```

If you are working from an LLVM checkout, copy `llvm/docs/tutorial/kaleidoscope/include/KaleidoscopeJIT.h` (or `llvm/examples/Kaleidoscope/include/KaleidoscopeJIT.h`) into `./include/KaleidoscopeJIT.h`.  Passing `-Iinclude` to `clang++` lets `#include "KaleidoscopeJIT.h"` find it cleanly.

---

## Inside `KaleidoscopeJIT` and Target `DataLayout`

Peek inside [KaleidoscopeJIT.h](../include/KaleidoscopeJIT.h) and you will see four key ORCv2 building blocks stacked together:

```text
+-------------------------------------------------------------------+
| llvm::orc::KaleidoscopeJIT                                        |
|                                                                   |
|   ExecutionSession ES  --->  MainJD (JITDylib symbol table)       |
|                              + DynamicLibrarySearchGenerator      |
|                                (resolves host C symbols: sin/cos) |
|                                                                   |
|   IRCompileLayer (ConcurrentIRCompiler -> TargetMachine)          |
|         |                                                         |
|         v                                                         |
|   RTDyldObjectLinkingLayer                                        |
|         |                                                         |
|         v                                                         |
|   SectionMemoryManager (allocates executable RAM pages)           |
+-------------------------------------------------------------------+
```

1.  **`ExecutionSession` and `MainJD` (`JITDylib`)**: Manages the in-memory dynamic symbol table.  `KaleidoscopeJIT::Create()` attaches `DynamicLibrarySearchGenerator::GetForCurrentProcess(...)` to `MainJD`, which means any `extern` function already loaded in our host process (such as `sin`, `cos`, `atan2`, or `pow` from `libm`) resolves automatically!
2.  **`DataLayout`**: Encodes the exact ABI layout rules of your CPU and OS: endianness, pointer bit-width, integer/float alignments, symbol mangling prefix (`_` on macOS Mach-O vs none on Linux ELF), and stack alignment.  Every `llvm::Module` handed to the JIT *must* adopt `TheJIT->getDataLayout()` so IR passes and code generation agree on memory layout and symbol names.

:::{note}
**Handling `llvm::Error` with `ExitOnError`**:
Factory methods like `KaleidoscopeJIT::Create()` return `llvm::Expected<std::unique_ptr<KaleidoscopeJIT>>` rather than throwing C++ exceptions (remember we compile with `-fno-rtti`, and LLVM also builds with `-fno-exceptions`).  LLVM's `llvm::ExitOnError` utility unwraps `Expected<T>` values cleanly during startup: if `Create()` succeeds, `ExitOnErr(...)` returns the `unique_ptr`; if host setup ever fails, it prints a diagnostic to `stderr` and exits cleanly.
:::

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply three updates on top of [Step 5.1](01-target-setup.md).

### 1. Include `"KaleidoscopeJIT.h"`

Add `#include "KaleidoscopeJIT.h"` as the very first line of `kaleidoscope.cpp`:

```cpp
#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
```

### 2. Add `TheJIT` and `ExitOnErr`, and Upgrade `InitializeModule()`

In the `Code Generation` section, add `TheJIT` and `ExitOnErr` right below `NamedValues`, and rename `InitializeModule()` to `InitializeModuleAndManagers()` (a name that anticipates adding optimisation pass managers in Section 9!), setting the module's `DataLayout` from `TheJIT`:

```cpp
static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::map<std::string, llvm::Value *> NamedValues;
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static llvm::ExitOnError ExitOnErr;

static void InitializeModuleAndManagers() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("KaleidoscopeJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}
```

### 3. Construct `TheJIT` and Print Its `DataLayout` in `main()`

Inside `main()`, replace `InitializeModule();` with creating `TheJIT` *first* (since `InitializeModuleAndManagers()` queries `TheJIT->getDataLayout()`!), calling `InitializeModuleAndManagers()`, printing the `DataLayout` string, and updating the banner to Step 5.2:

```cpp
  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  InitializeModuleAndManagers();
  fprintf(stderr, "JIT DataLayout: %s\n",
          TheJIT->getDataLayout().getStringRepresentation().c_str());

  printf("Kaleidoscope JIT REPL (Step 5.2: JIT Engine Configured)\n");
  printf("Type definitions, externs, or expressions followed by ';' (Ctrl+D to "
         "exit):\n\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference file:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -100,
  tok_extern = -101,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise identifiers and keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())) || LastChar == '_')
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  // Recognise numeric literals: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
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
  llvm::Value *codegen() override;
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}
  const std::string &getName() const { return Name; }
  llvm::Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  llvm::Value *codegen() override;
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(std::string Callee, std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(std::move(Callee)), Args(std::move(Args)) {}
  llvm::Value *codegen() override;
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

static std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static llvm::Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return Result;
}

static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat '('
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ')'
  return V;
}

static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  // Simple variable reference.
  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(std::move(IdName));

  // Function call.
  getNextToken(); // eat '('
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("expected ')' or ',' in argument list");
      getNextToken(); // eat ','
    }
  }

  getNextToken(); // eat ')'
  return std::make_unique<CallExprAST>(std::move(IdName), std::move(Args));
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}

/// binoprhs ::= (binop primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binary operator

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

static std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != tok_identifier)
    return LogErrorP("expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);

  if (CurTok != ')')
    return LogErrorP("expected ')' in prototype");

  getNextToken(); // eat ')'
  return std::make_unique<PrototypeAST>(std::move(FnName), std::move(ArgNames));
}

static std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken(); // eat 'def'
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

static std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    auto Proto = std::make_unique<PrototypeAST>("__anon_expr",
                                                std::vector<std::string>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat 'extern'
  return ParsePrototype();
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::map<std::string, llvm::Value *> NamedValues;
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static llvm::ExitOnError ExitOnErr;

static void InitializeModuleAndManagers() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("KaleidoscopeJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

llvm::Value *NumberExprAST::codegen() {
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
  llvm::Value *V = NamedValues[Name];
  if (!V)
    return LogErrorV("unknown variable name");
  return V;
}

llvm::Value *BinaryExprAST::codegen() {
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
    return LogErrorV("invalid binary operator");
  }
}

llvm::Value *CallExprAST::codegen() {
  llvm::Function *CalleeF = TheModule->getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("unknown function referenced");

  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("incorrect # arguments passed");

  std::vector<llvm::Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return nullptr;
  }

  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

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
  bool CreatedNew = false;
  llvm::Function *TheFunction = TheModule->getFunction(Proto->getName());
  if (!TheFunction) {
    TheFunction = Proto->codegen();
    CreatedNew = true;
  }
  if (!TheFunction)
    return nullptr;

  if (!TheFunction->empty()) {
    LogErrorV("function cannot be redefined in single-module IR mode");
    return nullptr;
  }

  auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;

  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
      if (CreatedNew)
        TheFunction->eraseFromParent();
      else
        TheFunction->deleteBody();
      return nullptr;
    }
    return TheFunction;
  }

  if (CreatedNew)
    TheFunction->eraseFromParent();
  else
    TheFunction->deleteBody();
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
  }
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
  }
}

static void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read top-level expression:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      FnIR->eraseFromParent();
    }
  } else {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
  }
}

static void Prompt() { fprintf(stderr, "ready> "); }

static void MainLoop() {
  while (true) {
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      Prompt();
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  fprintf(stderr, "Initialised native target: %s\n",
          llvm::sys::getDefaultTargetTriple().c_str());

  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  InitializeModuleAndManagers();
  fprintf(stderr, "JIT DataLayout: %s\n",
          TheJIT->getDataLayout().getStringRepresentation().c_str());

  printf("Kaleidoscope JIT REPL (Step 5.2: JIT Engine Configured)\n");
  printf("Type definitions, externs, or expressions followed by ';' (Ctrl+D to "
         "exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```
</details>

---

## Compiling and Running

Compile with `-Iinclude` and our Section 5 LLVM flags:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Run `./kaleidoscope` to see both the target triple and the `JIT DataLayout` string:

```bash
./kaleidoscope
```

```text
Initialised native target: x86_64-pc-linux-gnu
JIT DataLayout: e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128
Kaleidoscope JIT REPL (Step 5.2: JIT Engine Configured)
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> 42;
Read top-level expression:
define double @__anon_expr() {
entry:
  ret double 4.200000e+01
}

ready>
```

Decode a few pieces of that `DataLayout` string (`e-m:e-...-i64:64-...-n8:16:32:64-S128`):
* `e`: Little-endian byte order.
* `m:e`: ELF symbol mangling (on macOS you will see `m:o` for Mach-O `_` symbol prefixes!).
* `i64:64`: 64-bit integers are 64-bit aligned.
* `n8:16:32:64`: Native CPU register widths are 8, 16, 32, and 64 bits.
* `S128`: The stack pointer stays 128-bit (16-byte) aligned across calls, as required by the System V x86-64 ABI.

Now comes the payoff we have been building toward since Step 1.1: in [Step 5.3](03-executing-expressions.md), we will hand `@__anon_expr` to `TheJIT`, execute native machine code on your CPU, and print the result!

---

[Previous: Step 5.1 - Native Target Initialisation](01-target-setup.md) | [Section 5 Overview](index.md) | [Next: Step 5.3 - Executing Expressions](03-executing-expressions.md)
