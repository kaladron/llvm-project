# Step 5.4: Functions and `extern` Calls in the JIT

At the end of [Step 5.3](03-executing-expressions.md), we spotted why splitting each JIT submission into its own `llvm::Module` broke subsequent calls: every time `addModule()` takes ownership of `TheModule` and `InitializeModuleAndManagers()` creates a fresh empty module, any `extern` or `def` declarations that lived in the old module are no longer visible in the new `TheModule`!

In this final step of Milestone 2, we add two small maps (`FunctionProtos` and `FunctionRTs`) so functions and `extern` prototypes work seamlessly across modules **and** user-defined `def` functions can be redefined live in the REPL!

---

## Cross-Module Prototypes (`FunctionProtos`) and Live Redefinition (`FunctionRTs`)

Why can a new module not simply hold onto an `llvm::Function *` pointer from an older module?  Because in LLVM IR, every `llvm::Function` is owned by a single `llvm::Module`, and a `call` instruction inside Module 2 is only legal if its callee is declared (or defined) inside **Module 2** itself!

We solve both cross-module lookup and live function redefinition with two maps:

```text
  1. Cross-Module Calls via FunctionProtos
  ----------------------------------------
  Module 1 (JIT-compiled):   define double @foo(double %x) { ... }
                                  |
                                  +---> saves PrototypeAST in FunctionProtos["foo"]
                                                                    |
  Module 2 (new expression): getFunction("foo") -------------------+
                                  |  re-emits: declare double @foo(double)
                                  v
                             call double @foo(double 4.0)  --> linked by ORC JIT!

  2. Live Redefinition via FunctionRTs
  ------------------------------------
  First  "def foo(x) x + 1;"  --> compiled under RT_1, saved in FunctionRTs["foo"]
  Second "def foo(x) x * 10;" --> FunctionRTs["foo"]->remove() frees RT_1 first,
                                  then compiles new @foo under RT_2!
```

* **`FunctionProtos`**: Maps each function name to its `std::unique_ptr<PrototypeAST>`.  Whenever `getFunction("foo")` sees that `"foo"` is not yet declared in the *current* `TheModule`, it looks `"foo"` up in `FunctionProtos` and calls `Proto->codegen()` to emit a fresh `declare double @foo(...)` into the current `TheModule` on demand!
* **`FunctionRTs`**: Remember back in [Step 4.3](../04-llvm-ir/03-functions-ir.md) when single-module mode had to reject redefining a function (`!TheFunction->empty()`)?  In our JIT, each `def` compiles in its own module tracked by its own `ResourceTrackerSP` stored in `FunctionRTs[FnName]`.  If you type a brand-new `def foo(x)` with the same name, `HandleDefinition()` simply calls `It->second->remove()` to unload the old `@foo` symbol and machine code from `MainJD` before adding the new module!

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply six clean updates on top of [Step 5.3](03-executing-expressions.md).

### 1. Tidy Unused Host Header

Now that we verified our host target triple in Steps 5.1 through 5.3, remove `#include "llvm/TargetParser/Host.h"` from the top of `kaleidoscope.cpp` so every `#include` in the file earns its keep.

### 2. Add `FunctionProtos`, `FunctionRTs`, and `getFunction()` (With a Quick Stretch!)

In the `Code Generation` section, add `FunctionProtos` and `FunctionRTs` right below `TheJIT`, and write `getFunction(std::string Name)` right above `InitializeModuleAndManagers()`:

```cpp
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static std::map<std::string, llvm::orc::ResourceTrackerSP> FunctionRTs;
static llvm::ExitOnError ExitOnErr;

static llvm::Function *getFunction(std::string Name) {
  if (auto *F = TheModule->getFunction(Name))
    return F;

  // Your stretch: look up Name in FunctionProtos; if found, return
  // FI->second->codegen() to lazily emit its declaration in TheModule!

  return nullptr;
}
```

**Your Stretch**: Fill in the three lines inside `getFunction()` using `auto FI = FunctionProtos.find(Name);`: if `FI != FunctionProtos.end()`, `return FI->second->codegen();`!  Check your implementation against the fold below:

<details>
<summary>Click here to check your getFunction() helper</summary>

```cpp
static llvm::Function *getFunction(std::string Name) {
  if (auto *F = TheModule->getFunction(Name))
    return F;

  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}
```
</details>

### 3. Switch `CallExprAST::codegen()` to `getFunction()`

In `CallExprAST::codegen()`, change the single lookup line from `TheModule->getFunction(Callee)` to `getFunction(Callee)`:

```cpp
llvm::Value *CallExprAST::codegen() {
  llvm::Function *CalleeF = getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("unknown function referenced");
```

### 4. Simplify `FunctionAST::codegen()` Using `FunctionProtos`

Because every `def` now starts in a fresh empty `TheModule`, we no longer need `CreatedNew`, `deleteBody()`, or the single-module `!TheFunction->empty()` guard from [Step 4.3](../04-llvm-ir/03-functions-ir.md)!  Replace `FunctionAST::codegen()` with this streamlined version that caches `Proto` in `FunctionProtos` and calls `getFunction(P.getName())`:

```cpp
llvm::Function *FunctionAST::codegen() {
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  llvm::Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;

  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
      TheFunction->eraseFromParent();
      return nullptr;
    }
    return TheFunction;
  }

  TheFunction->eraseFromParent();
  return nullptr;
}
```

Notice why `auto &P = *Proto;` grabs a reference *before* `FunctionProtos[Proto->getName()] = std::move(Proto);`: moving `Proto` into the map empties the local `unique_ptr`, while the underlying `PrototypeAST` object now lives safely on the heap inside `FunctionProtos`, so `P.getName()` remains completely valid!

### 5. Ship `def` Modules to `TheJIT` (With `FunctionRTs`) and Cache `extern` Prototypes

Update `HandleDefinition()` and `HandleExtern()` in the `Interactive Driver Loop` section:
* In `HandleDefinition()`, remove any previous `ResourceTracker` for `FnName` before registering the new module under a fresh `RT`, then open a fresh `TheModule` with `InitializeModuleAndManagers()`.
* In `HandleExtern()`, save `ProtoAST` into `FunctionProtos` so future modules can re-declare the external function whenever needed!

```cpp
static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      std::string FnName = std::string(FnIR->getName());
      fprintf(stderr, "Read function definition:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");

      // If redefining an existing JIT function, remove its previous code first.
      if (auto It = FunctionRTs.find(FnName); It != FunctionRTs.end()) {
        ExitOnErr(It->second->remove());
        FunctionRTs.erase(It);
      }

      auto RT = TheJIT->getMainJITDylib().createResourceTracker();
      ExitOnErr(
          TheJIT->addModule(llvm::orc::ThreadSafeModule(std::move(TheModule),
                                                        std::move(TheContext)),
                            RT));
      FunctionRTs[FnName] = std::move(RT);
      InitializeModuleAndManagers();
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
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
  }
}
```

### 6. Tidy Startup Logs in `main()`

In `main()`, remove the two temporary startup `fprintf(stderr, ...)` lines (`Initialised native target:` and `JIT DataLayout:`) from Steps 5.1 and 5.2, and update the banner to `Step 5.4: Complete JIT`:

```cpp
int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  InitializeModuleAndManagers();

  printf("Kaleidoscope JIT REPL (Step 5.4: Complete JIT)\n");
  printf("Type definitions, externs, or expressions followed by ';' (Ctrl+D to "
         "exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete Milestone 2 reference file:

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
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static std::map<std::string, llvm::orc::ResourceTrackerSP> FunctionRTs;
static llvm::ExitOnError ExitOnErr;

static llvm::Function *getFunction(std::string Name) {
  if (auto *F = TheModule->getFunction(Name))
    return F;

  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}

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
  llvm::Function *CalleeF = getFunction(Callee);
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
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  llvm::Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;

  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
      TheFunction->eraseFromParent();
      return nullptr;
    }
    return TheFunction;
  }

  TheFunction->eraseFromParent();
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      std::string FnName = std::string(FnIR->getName());
      fprintf(stderr, "Read function definition:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");

      // If redefining an existing JIT function, remove its previous code first.
      if (auto It = FunctionRTs.find(FnName); It != FunctionRTs.end()) {
        ExitOnErr(It->second->remove());
        FunctionRTs.erase(It);
      }

      auto RT = TheJIT->getMainJITDylib().createResourceTracker();
      ExitOnErr(
          TheJIT->addModule(llvm::orc::ThreadSafeModule(std::move(TheModule),
                                                        std::move(TheContext)),
                            RT));
      FunctionRTs[FnName] = std::move(RT);
      InitializeModuleAndManagers();
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
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
  }
}

static void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      auto RT = TheJIT->getMainJITDylib().createResourceTracker();

      auto TSM = llvm::orc::ThreadSafeModule(std::move(TheModule),
                                             std::move(TheContext));
      ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
      InitializeModuleAndManagers();

      if (auto ExprSymbol = TheJIT->lookup("__anon_expr")) {
        double (*FP)() = ExprSymbol->getAddress().toPtr<double (*)()>();
        fprintf(stderr, "Evaluated to %f\n", FP());
      } else {
        llvm::logAllUnhandledErrors(ExprSymbol.takeError(), llvm::errs(),
                                    "JIT Error: ");
      }

      ExitOnErr(RT->remove());
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

  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  InitializeModuleAndManagers();

  printf("Kaleidoscope JIT REPL (Step 5.4: Complete JIT)\n");
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

## Compiling and Testing Live Function Redefinition!

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Run `./kaleidoscope`, define `foo(x)`, evaluate `foo(4);`, **redefine `foo(x)` live**, and call `foo(4);` again:

```bash
./kaleidoscope
```

```text
Kaleidoscope JIT REPL (Step 5.4: Complete JIT)
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> def foo(x) x + 1;
Read function definition:
define double @foo(double %x) {
entry:
  %addtmp = fadd double %x, 1.000000e+00
  ret double %addtmp
}

ready> foo(4);
Evaluated to 5.000000
ready> def foo(x) x * 10;
Read function definition:
define double @foo(double %x) {
entry:
  %multmp = fmul double %x, 10.000000e+00
  ret double %multmp
}

ready> foo(4);
Evaluated to 40.000000
ready>
```

Look at that: `foo(4);` returned `5.000000`, then we replaced `def foo(x)` on the fly so `FunctionRTs["foo"]->remove()` unloaded the first implementation, and `foo(4);` immediately evaluated to `40.000000`!

:::{note}
**Direct Calls vs Already-Materialised Callers During Redefinition**:
Notice that each top-level call `foo(4);` compiles a fresh `@__anon_expr`, which looks up the *latest* `@foo` address in `MainJD`.  What if an older function `def bar(y) foo(y);` had *already* been materialised into machine code before we removed the old `foo`?  Because `KaleidoscopeJIT` links eagerly through `RTDyldObjectLinkingLayer`, `bar`'s machine-code `call` instruction was already patched directly to the old `foo` memory page!  Hot-swapping callees underneath already-compiled callers requires routing calls through an indirect stub table or lazy call-through manager, which LLVM's dedicated *Building a JIT* tutorial explores in detail.
:::

---

## Active Learning Challenge: Verifying a Trigonometric Identity in Native Code

Put external C math calls and user-defined JIT functions together to verify the classic trigonometric identity `sin(x) * sin(x) + cos(x) * cos(x) = 1` across several inputs!

### The Challenge

In your running `./kaleidoscope` REPL:
1.  Declare `extern sin(x);` and `extern cos(x);`.
2.  Define `def trig_id(x) sin(x) * sin(x) + cos(x) * cos(x);`.
3.  Evaluate `trig_id(0.5);` and `trig_id(3.14159);` and verify that native `libm` calls plus your JIT-compiled `fmul`/`fadd` instructions return `1.000000` every time!

Try it in your terminal before opening the transcript fold below.

<details>
<summary>Click here to compare your trigonometric JIT session</summary>

```text
ready> extern sin(x);
Read extern:
declare double @sin(double)

ready> extern cos(x);
Read extern:
declare double @cos(double)

ready> def trig_id(x) sin(x) * sin(x) + cos(x) * cos(x);
Read function definition:
define double @trig_id(double %x) {
entry:
  %calltmp = call double @sin(double %x)
  %calltmp1 = call double @sin(double %x)
  %multmp = fmul double %calltmp, %calltmp1
  %calltmp2 = call double @cos(double %x)
  %calltmp3 = call double @cos(double %x)
  %multmp4 = fmul double %calltmp2, %calltmp3
  %addtmp = fadd double %multmp, %multmp4
  ret double %addtmp
}

ready> trig_id(0.5);
Evaluated to 1.000000
ready> trig_id(3.14159);
Evaluated to 1.000000
ready>
```

(Look at `@trig_id`'s IR too: right now it calls `@sin(double %x)` twice and `@cos(double %x)` twice!  Keep that mental snapshot handy for later chapters when LLVM optimisation passes enter the picture.)
</details>

---

## Milestone 2 Complete!

Take a step back and look at what your ~580-line `kaleidoscope.cpp` can now do from scratch:
* Scan source text into tokens while skipping whitespace and `#` comments ([Section 1](../01-lexer/index.md)).
* Parse primary expressions, function calls, prototypes, and precedence-climbed binary expressions into an AST ([Section 2](../02-parser-ast/index.md) and [Section 3](../03-operators-eval/index.md)).
* Emit verified SSA LLVM IR for constants, arithmetic, comparisons, functions, and external C declarations ([Section 4](../04-llvm-ir/index.md)).
* Compile IR on demand into native host machine code via ORCv2, resolve C library symbols dynamically, execute functions at hardware speed, and redefine functions live ([Section 5](index.md)).

Right now, every function in Kaleidoscope executes a single straight-line `entry` basic block from top to bottom.  In [Section 6: Control Flow (If / Then / Else)](../06-control-flow/index.md), we will bring back those `if`, `then`, and `else` keywords from your Step 1.5 challenge, split functions across multiple basic blocks, and learn how SSA merges control-flow paths using `PHI` nodes!

---

[Previous: Step 5.3 - Executing Expressions](03-executing-expressions.md) | [Section 5 Overview](index.md) | [Next: Section 6 - Control Flow](../06-control-flow/index.md)
