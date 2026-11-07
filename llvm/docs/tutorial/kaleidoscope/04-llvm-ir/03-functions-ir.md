# Step 4.3: Function Definitions

In [Step 4.2](02-binary-ir.md), `IRBuilder` folded `1 + 2 * 3;` straight down to `7.0` because every operand was a compile-time constant.  Now we add user-defined functions like `def square(x) x * x;` and `def linear(m x b) m * x + b;`, introducing function prototypes, parameter symbol tables, and LLVM's IR verifier!

---

## Prototypes vs Function Bodies

Kaleidoscope splits a function into two AST nodes (neither of which inherits from `ExprAST`, because functions are top-level declarations rather than expressions that evaluate to a `double`):
1.  **`PrototypeAST`**: Captures the function's name and its parameter names (for example, `linear(m x b)`).  Reusing `PrototypeAST` on its own in [Step 4.4](04-calls-and-extern.md) will give us `extern sin(x);` almost for free!
2.  **`FunctionAST`**: Combines a `PrototypeAST` signature with an `ExprAST` body expression.

```text
+--------------------------------+      codegen()      +-------------------------------------+
| PrototypeAST: "square(x)"      | ------------------> | declare/define double @square(%x)   |
+--------------------------------+                     +-------------------------------------+
                |                                                        |
                v  (wrapped inside FunctionAST)                          v
+--------------------------------+      codegen()      +-------------------------------------+
| Body ExprAST: (* (Var x)       | ------------------> | entry:                              |
|                  (Var x))      |                     |   %multmp = fmul double %x, %x      |
+--------------------------------+                     |   ret double %multmp                |
                                                       +-------------------------------------+
                                                                         |
                                                                         v
                                                       +-------------------------------------+
                                                       | llvm::verifyFunction(*TheFunction)  |
                                                       +-------------------------------------+
```

:::{note}
**Space-Separated Parameter Lists in Prototypes**:
Notice Kaleidoscope's classic syntax quirk: function *call* arguments from [Step 2.4](../02-parser-ast/04-function-calls.md) are separated by commas (`linear(2, 3, 1)`), because each argument can be an arbitrary compound expression like `a + 1`.  Function *prototype* parameters, by contrast, can only be bare identifier names, so Kaleidoscope separates them simply with spaces: `def linear(m x b) m * x + b;`!
:::

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply five updates on top of [Step 4.2](02-binary-ir.md).

### 1. Add `PrototypeAST` and `FunctionAST`

At the bottom of the `Abstract Syntax Tree (AST)` section (directly below `CallExprAST`), add `PrototypeAST` and `FunctionAST`:

```cpp
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

Notice that both `codegen()` methods return `llvm::Function *` rather than `llvm::Value *`, since callers inspect and print the generated `Function` directly.

### 2. Parse Prototypes, Definitions, and Anonymous Top-Level Wrappers

At the bottom of the `Parser` section (directly below `ParseExpression()`), add `LogErrorP()`, `ParsePrototype()`, `ParseDefinition()`, and `ParseTopLevelExpr()`:

```cpp
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
```

Look how clean `ParseTopLevelExpr()` is: instead of manually building `__anon_expr`'s `FunctionType` and `BasicBlock` inside the REPL loop like we did in Steps 4.1 and 4.2, a top-level expression simply wraps itself in a zero-argument `FunctionAST` named `"__anon_expr"` and reuses `FunctionAST::codegen()` (including IR verification)!

### 3. Add `NamedValues` and Implement `VariableExprAST::codegen()`

In the `Code Generation` section, right below `Builder`, add our parameter symbol table `NamedValues`:

```cpp
static std::map<std::string, llvm::Value *> NamedValues;
```

Right now, the only variables in scope inside a function are its parameters (in Section 9, we will upgrade `NamedValues` to hold mutable stack slots via `alloca`).  Replace the Step 4.1 stub of `VariableExprAST::codegen()` so it looks up `Name` in `NamedValues`:

```cpp
llvm::Value *VariableExprAST::codegen() {
  llvm::Value *V = NamedValues[Name];
  if (!V)
    return LogErrorV("unknown variable name");
  return V;
}
```

### 4. Implement `PrototypeAST::codegen()` and `FunctionAST::codegen()`

Directly below `CallExprAST::codegen()`, add code generation for prototypes and full functions:

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
```

Take a moment to appreciate three defensive checks inside `FunctionAST::codegen()`:
1.  **Single-Module Redefinition Guard (`!TheFunction->empty()`)**: An `llvm::Function` with zero basic blocks (`empty() == true`) is a body-less `extern` declaration, so attaching a `def` body to it is allowed.  An `llvm::Function` that *already* has basic blocks (`!TheFunction->empty()`) already has a body in `TheModule`, so appending a second `"entry"` block into the same function would corrupt it!  Right now all functions live in a single `TheModule`, so we reject redefinition cleanly; in [Step 5.4](../05-jit/04-functions-and-externs.md), giving each JIT compile its own `Module` and `ResourceTracker` will let you redefine functions interactively!
2.  **`llvm::verifyFunction(*TheFunction, &llvm::errs())` Returns `true` on Failure**: Watch out for LLVM's inverted boolean convention here: `verifyFunction` returns `false` when the function is **valid** and `true` when it finds **broken IR**!  Checking the return value ensures broken IR never lingers in `TheModule`.
3.  **Clean Rollback (`CreatedNew`)**: If `Body->codegen()` fails (say you typed `def bad(x) y;` referencing an unknown variable `y`) or verification fails, `eraseFromParent()` deletes a newly created function completely, while `deleteBody()` strips only the failed basic block if the signature came from an earlier `extern` prototype!

### 5. Wire `HandleDefinition()` into `MainLoop()`

In the `Interactive Driver Loop` section, add `HandleDefinition()`, simplify `HandleTopLevelExpression()` to use `ParseTopLevelExpr()`, add `case tok_def:` to `MainLoop()`, and update `main()`'s banner:

```cpp
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
```

Inside `MainLoop()`, add `case tok_def:` right below `case ';':`:

```cpp
    case tok_def:
      HandleDefinition();
      break;
```

And update the two `printf` lines in `main()`:

```cpp
  printf("Kaleidoscope LLVM IR Generator (Step 4.3: Functions)\n");
  printf("Type function definitions or expressions followed by ';' (Ctrl+D to "
         "exit):\n\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference file:

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
  return LogErrorV("function call codegen arrives in Step 4.4");
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

  printf("Kaleidoscope LLVM IR Generator (Step 4.3: Functions)\n");
  printf("Type function definitions or expressions followed by ';' (Ctrl+D to "
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

Run `./kaleidoscope` and define both single-parameter and multi-parameter functions:

```bash
./kaleidoscope
```

```text
Kaleidoscope LLVM IR Generator (Step 4.3: Functions)
Type function definitions or expressions followed by ';' (Ctrl+D to exit):

ready> def square(x) x * x;
Read function definition:
define double @square(double %x) {
entry:
  %multmp = fmul double %x, %x
  ret double %multmp
}

ready> def linear(m x b) m * x + b;
Read function definition:
define double @linear(double %m, double %x, double %b) {
entry:
  %multmp = fmul double %m, %x
  %addtmp = fadd double %multmp, %b
  ret double %addtmp
}

ready>
```

There they are!  Because `%m`, `%x`, and `%b` are runtime SSA arguments rather than constants, `IRBuilder` emitted `%multmp = fmul double %m, %x` followed by `%addtmp = fadd double %multmp, %b` in exact precedence order!

In [Step 4.4](04-calls-and-extern.md), we will implement `CallExprAST::codegen()` and `extern` declarations so functions and top-level expressions can call both your own `def` functions and C math library routines.

---

[Previous: Step 4.2 - Binary Math in IR](02-binary-ir.md) | [Section 4 Overview](index.md) | [Next: Step 4.4 - Function Calls and `extern`](04-calls-and-extern.md)
