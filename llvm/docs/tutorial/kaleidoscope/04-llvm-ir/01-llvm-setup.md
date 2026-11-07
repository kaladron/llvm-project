# Step 4.1: LLVM Setup and Constant IR

Welcome to Section 4!  Right now, your `kaleidoscope.cpp` from [Step 3.4](../03-operators-eval/04-immediate-eval.md) parses full expressions and evaluates them in C++ with `eval()`.  In this step, we swap `print()` and `eval()` for `llvm::Value *codegen()`, initialise our core LLVM objects, and emit our very first valid LLVM IR function!

---

## The Three Core LLVM Objects

Every LLVM IR frontend revolves around three objects that work together:

1.  **`llvm::LLVMContext`**: Owns LLVM's core uniquing tables for types (`double`, `i1`) and constants (`42.0`).
2.  **`llvm::Module`**: Represents a single translation unit, owning all generated functions and global variables.
3.  **`llvm::IRBuilder<>`**: Acts like a smart cursor pointing at the end of a `BasicBlock`, creating SSA instructions and appending them in order.

```text
+---------------------------------------------------------------+
| LLVMContext (owns type tables & constant pools)               |
|                                                               |
|   +-------------------------------------------------------+   |
|   | Module: "Kaleidoscope" (translation unit)             |   |
|   |                                                       |   |
|   |   +-----------------------------------------------+   |   |
|   |   | Function: @__anon_expr()                      |   |   |
|   |   |   +---------------------------------------+   |   |   |
|   |   |   | BasicBlock: "entry"                   | <======= IRBuilder<>
|   |   |   |   ret double 4.200000e+01             |   |   |  (SetInsertPoint)
|   |   |   +---------------------------------------+   |   |   |
|   |   +-----------------------------------------------+   |   |
|   +-------------------------------------------------------+   |
+---------------------------------------------------------------+
```

What is `llvm::Value *`, the return type of `codegen()`?  In Static Single Assignment (SSA) form, every constant, function argument, and instruction result is computed once and never mutated.  LLVM represents any computed SSA value with the base class `llvm::Value`.

:::{note}
**`LLVMContext` and Thread Safety**:
An `llvm::LLVMContext` is intentionally single-threaded so type and constant lookups require no locks.  If you ever build a multi-threaded compiler that compiles several files in parallel, give each worker thread its own independent `LLVMContext`!
:::

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file from [Step 3.4](../03-operators-eval/04-immediate-eval.md).  Instead of throwing away our parser, we keep 100% of our lexer and precedence-climbing parser intact and make five clean updates.

### 1. Include LLVM Headers (and Drop `<iostream>`)

At the very top of `kaleidoscope.cpp`, add the twelve LLVM headers we will use across Section 4, and remove `#include <iostream>` (since LLVM provides its own fast `llvm::errs()` stream via `raw_ostream.h`):

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
```

### 2. Replace `print()` and `eval()` on the AST with `codegen()`

In the `Abstract Syntax Tree (AST)` section, replace `print()` and `eval()` across `ExprAST` and its four subclasses with `llvm::Value *codegen()` declarations (plus a `getName()` accessor on `VariableExprAST` that will prove handy later):

```cpp
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
```

### 3. Add `LogErrorV()` in the Parser Section

Right below `LogError()` in the `Parser` section, add a tiny helper that logs the same error message and returns `nullptr` typed as an `llvm::Value *`:

```cpp
static llvm::Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}
```

### 4. Add the `Code Generation` Section

Directly below `ParseExpression()` (and above the `Interactive Driver Loop` banner), create a new `Code Generation` section containing our LLVM state, `NumberExprAST::codegen()`, and clear step-by-step placeholder stubs for the other three AST nodes:

```cpp
//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

static void InitializeModule() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("Kaleidoscope", *TheContext);
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

llvm::Value *NumberExprAST::codegen() {
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
  return LogErrorV("variable codegen arrives in Step 4.3");
}

llvm::Value *BinaryExprAST::codegen() {
  (void)Op;
  return LogErrorV("binary operator codegen arrives in Step 4.2");
}

llvm::Value *CallExprAST::codegen() {
  return LogErrorV("function call codegen arrives in Step 4.4");
}
```

Look at two neat details here:
* In LLVM IR, floating-point constants are represented by `llvm::ConstantFP` backed by `llvm::APFloat` (Arbitrary-Precision Float, which models IEEE-754 arithmetic accurately across host architectures).
* Why `(void)Op;` inside `BinaryExprAST::codegen()`?  Because `LHS` and `RHS` are touched by `BinaryExprAST`'s destructor (`std::unique_ptr`), whereas `char Op` is a plain private field not yet read until Step 4.2; `(void)Op;` keeps `-Wall -Wextra` (`-Wunused-private-field`) completely silent!

### 5. Wrap Top-Level Expressions in `@__anon_expr`

In LLVM IR, instructions like `ret` must live inside a `BasicBlock` belonging to a `Function`.  To inspect top-level expressions typed at the `ready> ` prompt, we wrap each expression's `RetVal` in a zero-argument anonymous function `double @__anon_expr()`, print its IR with `Fn->print(llvm::errs())`, and then remove the temporary wrapper from `TheModule` with `Fn->eraseFromParent()`.  (In Section 5, our JIT will compile and *call* `@__anon_expr()` to compute the live answer!)

In the `Interactive Driver Loop` section, add `HandleTopLevelExpression()`, call it from `MainLoop()`, and call `InitializeModule()` inside `main()`:

```cpp
static void HandleTopLevelExpression() {
  if (auto E = ParseExpression()) {
    if (auto *RetVal = E->codegen()) {
      auto *FT =
          llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), false);
      auto *Fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "__anon_expr", TheModule.get());
      auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", Fn);
      Builder->SetInsertPoint(BB);
      Builder->CreateRet(RetVal);

      fprintf(stderr, "Read top-level expression:\n");
      Fn->print(llvm::errs());
      fprintf(stderr, "\n");
      Fn->eraseFromParent();
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

  printf("Kaleidoscope LLVM IR Generator (Step 4.1: Constant IR)\n");
  printf("Type numbers followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
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

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

static void InitializeModule() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("Kaleidoscope", *TheContext);
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

llvm::Value *NumberExprAST::codegen() {
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegen() {
  return LogErrorV("variable codegen arrives in Step 4.3");
}

llvm::Value *BinaryExprAST::codegen() {
  (void)Op;
  return LogErrorV("binary operator codegen arrives in Step 4.2");
}

llvm::Value *CallExprAST::codegen() {
  return LogErrorV("function call codegen arrives in Step 4.4");
}

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void HandleTopLevelExpression() {
  if (auto E = ParseExpression()) {
    if (auto *RetVal = E->codegen()) {
      auto *FT =
          llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), false);
      auto *Fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "__anon_expr", TheModule.get());
      auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", Fn);
      Builder->SetInsertPoint(BB);
      Builder->CreateRet(RetVal);

      fprintf(stderr, "Read top-level expression:\n");
      Fn->print(llvm::errs());
      fprintf(stderr, "\n");
      Fn->eraseFromParent();
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

  printf("Kaleidoscope LLVM IR Generator (Step 4.1: Constant IR)\n");
  printf("Type numbers followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```
</details>

---

## Compiling and Running

Compile linking against LLVM's `core` component:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Run `./kaleidoscope` and test both a number and a binary expression:

```bash
./kaleidoscope
```

```text
Kaleidoscope LLVM IR Generator (Step 4.1: Constant IR)
Type numbers followed by ';' and press Enter (Ctrl+D to exit):

ready> 42;
Read top-level expression:
define double @__anon_expr() {
entry:
  ret double 4.200000e+01
}

ready> 1 + 2;
Error: binary operator codegen arrives in Step 4.2
ready>
```

You just emitted your first genuine LLVM IR function (`define double @__anon_expr()`), complete with an `entry:` basic block and a typed `ret double 4.200000e+01` terminator!

In [Step 4.2](02-binary-ir.md), we will replace that `BinaryExprAST::codegen()` stub with real floating-point arithmetic and comparison instructions.

---

[Previous: Step 3.4 - Immediate Evaluation](../03-operators-eval/04-immediate-eval.md) | [Section 4 Overview](index.md) | [Next: Step 4.2 - Binary Math in IR](02-binary-ir.md)
