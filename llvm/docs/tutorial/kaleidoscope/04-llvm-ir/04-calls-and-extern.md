# Step 4.4: Function Calls, `extern`, and Hands-On IR Challenge

In this final step of Section 4, we replace our last remaining placeholder stub (`CallExprAST::codegen()`) and wire up the `extern` keyword so Kaleidoscope code can call both user-defined `def` functions and external C library functions like `sin`, `cos`, `atan2`, and `pow`.

---

## How Function Calls and `extern` Prototypes Work Together

In LLVM IR, there is no structural difference between calling a function you wrote in Kaleidoscope and calling a function compiled into the C math library (`libm`):
* A `def` function has basic blocks (`define double @square(double %x) { ... }`).
* An `extern` declaration is simply an `llvm::Function` with **zero basic blocks** (`declare double @sin(double)`), which `PrototypeAST::codegen()` from [Step 4.3](03-functions-ir.md) *already* produces!

When `CallExprAST::codegen()` compiles `foo(a, b)`, it walks four steps:

```text
+--------------------------------------------------+
| 1. TheModule->getFunction(Callee)                |
|    Found?  No  --> LogErrorV("unknown function") |
+--------------------------------------------------+
                         | Yes
                         v
+--------------------------------------------------+
| 2. CalleeF->arg_size() == Args.size()?           |
|    Match?  No  --> LogErrorV("incorrect # args") |
+--------------------------------------------------+
                         | Yes
                         v
+--------------------------------------------------+
| 3. Recursively codegen() each Args[i] into ArgsV |
+--------------------------------------------------+
                         | All non-null
                         v
+--------------------------------------------------+
| 4. Builder->CreateCall(CalleeF, ArgsV, "calltmp")|
+--------------------------------------------------+
```

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply three updates on top of [Step 4.3](03-functions-ir.md).

### 1. Add `ParseExtern()` to the Parser

Directly below `ParseTopLevelExpr()` at the end of the `Parser` section, add `ParseExtern()`.  Because an `extern` statement is just the `extern` keyword followed by a prototype (`extern sin(x);`), it is three lines long:

```cpp
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat 'extern'
  return ParsePrototype();
}
```

### 2. Stretch: Implement `CallExprAST::codegen()`

Replace the Step 4.1 stub of `CallExprAST::codegen()` in the `Code Generation` section.  Try writing it yourself first using the four-step diagram above:
1.  Look up `llvm::Function *CalleeF = TheModule->getFunction(Callee);`.  If `!CalleeF`, `return LogErrorV("unknown function referenced");`.
2.  Check `if (CalleeF->arg_size() != Args.size())`, returning `LogErrorV("incorrect # arguments passed");` on a mismatch.
3.  Build `std::vector<llvm::Value *> ArgsV;` by pushing `Args[i]->codegen()` for each argument; if `ArgsV.back()` is `nullptr`, `return nullptr;`.
4.  Return `Builder->CreateCall(CalleeF, ArgsV, "calltmp");`.

Check your work against the fold below!

<details>
<summary>Click here to check your CallExprAST::codegen()</summary>

```cpp
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
```
</details>

### 3. Add `HandleExtern()` and Wire `tok_extern` into `MainLoop()`

In the `Interactive Driver Loop` section, add `HandleExtern()` right below `HandleDefinition()`:

```cpp
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
```

Inside `MainLoop()`, add `case tok_extern:` right below `case tok_def:`:

```cpp
    case tok_extern:
      HandleExtern();
      break;
```

And update the two `printf` lines in `main()`:

```cpp
  printf("Kaleidoscope LLVM IR Generator (Step 4.4: Complete Codegen)\n");
  printf("Type definitions, externs, or expressions followed by ';' (Ctrl+D to "
         "exit):\n\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete Section 4 reference file:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
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

static void InitializeModule() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("Kaleidoscope", *TheContext);
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
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  InitializeModule();

  printf("Kaleidoscope LLVM IR Generator (Step 4.4: Complete Codegen)\n");
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

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Run `./kaleidoscope`, declare `extern sin(x);`, and call it from a top-level expression:

```bash
./kaleidoscope
```

```text
Kaleidoscope LLVM IR Generator (Step 4.4: Complete Codegen)
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> extern sin(x);
Read extern:
declare double @sin(double)

ready> sin(1.0);
Read top-level expression:
define double @__anon_expr() {
entry:
  %calltmp = call double @sin(double 1.000000e+00)
  ret double %calltmp
}

ready>
```

Notice why `IRBuilder` did *not* constant-fold `sin(1.0)` the way it folded `1 + 2 * 3` in [Step 4.2](02-binary-ir.md): `@sin` is an external function call, so `Builder->CreateCall` emitted `%calltmp = call double @sin(double 1.000000e+00)`!

---

## Hands-On IR Experiment: Multi-Argument C Math Functions and SSA Naming

Put your complete IR generator to work with multi-argument functions from the C standard math library (`<math.h>`):

1.  In your running `./kaleidoscope` session, declare two-argument C math functions `atan2` and `pow` (remember from [Step 4.3](03-functions-ir.md) that prototype parameter names are separated by **spaces**, not commas!):
    * `extern atan2(y x);`
    * `extern pow(a b);`
2.  Now define a Kaleidoscope function `hypot_sq(x y)` that calls `pow` *twice* in the same expression (remember call arguments use **commas**!):
    * `def hypot_sq(x y) pow(x, 2) + pow(y, 2);`
3.  Finally, call `atan2(1.0, 1.0);` at the top level, and also try passing the wrong number of arguments (`atan2(1.0);`) to verify your arity check!

Predict what SSA register names `IRBuilder` will pick when `"calltmp"` is requested twice inside `hypot_sq`, then compare your terminal output with the fold below!

<details>
<summary>Click here to compare your multi-argument IR session</summary>

```text
ready> extern atan2(y x);
Read extern:
declare double @atan2(double, double)

ready> extern pow(a b);
Read extern:
declare double @pow(double, double)

ready> def hypot_sq(x y) pow(x, 2) + pow(y, 2);
Read function definition:
define double @hypot_sq(double %x, double %y) {
entry:
  %calltmp = call double @pow(double %x, double 2.000000e+00)
  %calltmp1 = call double @pow(double %y, double 2.000000e+00)
  %addtmp = fadd double %calltmp, %calltmp1
  ret double %addtmp
}

ready> atan2(1.0, 1.0);
Read top-level expression:
define double @__anon_expr() {
entry:
  %calltmp = call double @atan2(double 1.000000e+00, double 1.000000e+00)
  ret double %calltmp
}

ready> atan2(1.0);
Error: incorrect # arguments passed
ready>
```

Look at `@hypot_sq`: because SSA forbids assigning to `%calltmp` twice, LLVM's value symbol table automatically numbered the second call `%calltmp1` and wired `%addtmp = fadd double %calltmp, %calltmp1`!
</details>

---

## Next Steps

You now have a complete frontend and LLVM IR generator for expressions, functions, and external C calls.  Right now, `@__anon_expr()` prints its IR and immediately erases itself without running.

In [Section 5: The JIT (Executing Native Machine Code)](../05-jit/index.md), we will plug LLVM's ORC JIT engine into `kaleidoscope.cpp` so `sin(1.0);` and `atan2(1.0, 1.0);` compile to native machine instructions in memory and print `0.841471` and `0.785398` live!

---

[Previous: Step 4.3 - Function Definitions](03-functions-ir.md) | [Section 4 Overview](index.md) | [Next: Section 5 - The JIT](../05-jit/index.md)
