# Step 4.2: Binary Math in IR

In [Step 4.1](01-llvm-setup.md), typing `1 + 2;` hit our placeholder stub (`binary operator codegen arrives in Step 4.2`).  Now we replace that stub so `BinaryExprAST::codegen()` emits LLVM floating-point arithmetic and comparison instructions.

---

## Floating-Point Instructions in LLVM IR

LLVM separates integer arithmetic (`add`, `sub`, `mul`, `sdiv`) from IEEE-754 floating-point arithmetic (`fadd`, `fsub`, `fmul`, `fdiv`).  Because every value in Kaleidoscope so far is a 64-bit `double`, each operator maps to a floating-point `IRBuilder` method:

| Operator | LLVM Instruction | `IRBuilder` Call |
| :--- | :--- | :--- |
| `+` | `fadd double` | `Builder->CreateFAdd(L, R, "addtmp")` |
| `-` | `fsub double` | `Builder->CreateFSub(L, R, "subtmp")` |
| `*` | `fmul double` | `Builder->CreateFMul(L, R, "multmp")` |
| `/` | `fdiv double` | `Builder->CreateFDiv(L, R, "divtmp")` |
| `<` | `fcmp ult double` + `uitofp i1 ... to double` | `CreateFCmpULT` + `CreateUIToFP` |

Notice the string arguments `"addtmp"`, `"multmp"`, and `"cmptmp"`.  Those are optional SSA **name hints**: when LLVM prints IR, it names the resulting registers `%addtmp`, `%multmp`, `%addtmp1`, and so on, making the text output far easier for humans to read than numbered temporaries (`%0`, `%1`, `%2`)!

### Two Important Subtleties on `<` Comparisons

1.  **Why `uitofp i1 ... to double`?**  `CreateFCmpULT` returns a **1-bit integer** (`i1`: `0` for false, `1` for true), *not* a `double`.  Because Kaleidoscope has no separate `bool` type yet, every expression must evaluate to a `double` (`0.0` or `1.0`).  Passing the `i1` into `Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp")` zero-extends `0` to `0.0` and `1` to `1.0`!  (Notice we use **unsigned** `UIToFP`, not signed `SIToFP`: in 1-bit two's complement, signed `1` would become `-1.0`!)
2.  **`fcmp ult` vs `fcmp olt` (`NaN` Behaviour)**:  In IEEE-754 floating point, comparing against Not-a-Number (`NaN`, such as `0.0 / 0.0` from [Step 3.4](../03-operators-eval/04-immediate-eval.md)) is *unordered*.  LLVM offers two flavours of less-than:
    * `fcmp olt` (**ordered** less-than) evaluates to `true` only if *neither* operand is `NaN` **and** `L < R` (matching C++'s `<` operator).
    * `fcmp ult` (**unordered** less-than) evaluates to `true` if *either* operand is `NaN` **or** `L < R`.
    Classic Kaleidoscope uses `CreateFCmpULT` (`fcmp ult`), and for all ordinary numbers (`3 < 5`) `ult` and `olt` behave identically; keep `olt` in mind whenever you build languages that require strict C-style `NaN` comparisons!

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply three updates on top of [Step 4.1](01-llvm-setup.md).

### 1. Stretch: Implement `BinaryExprAST::codegen()`

Recall how you wrote `BinaryExprAST::eval()` in [Step 3.4](../03-operators-eval/04-immediate-eval.md).  `BinaryExprAST::codegen()` has the exact same recursive structure!  Here it is with `'+'` and `'-'` filled in:

```cpp
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
  // Your stretch: implement '*', '/', and '<' (CreateFCmpULT + CreateUIToFP)!
  default:
    return LogErrorV("invalid binary operator");
  }
}
```

**Your Stretch**: Replace the stub `BinaryExprAST::codegen()` from Step 4.1 and fill in `case '*':`, `case '/':`, and `case '<':` using the table above!  Expand the fold below to check your implementation.

<details>
<summary>Click here to check your BinaryExprAST::codegen()</summary>

```cpp
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
```
</details>

### 2. Set the `IRBuilder` Insert Point *Before* Calling `E->codegen()`

Look closely at `HandleTopLevelExpression()` from [Step 4.1](01-llvm-setup.md): we called `E->codegen()` *before* creating `@__anon_expr` and calling `Builder->SetInsertPoint(BB)`.  That worked for single constants because `ConstantFP::get` creates no instructions, but `Builder->CreateFAdd(...)` needs an active `BasicBlock` insertion point whenever it emits an instruction!

Update `HandleTopLevelExpression()` so `@__anon_expr` and its `"entry"` block are created *before* `E->codegen()`, and `Fn->eraseFromParent()` cleans up `@__anon_expr` whether `codegen()` succeeds or fails:

```cpp
static void HandleTopLevelExpression() {
  if (auto E = ParseExpression()) {
    auto *FT =
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), false);
    auto *Fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                      "__anon_expr", TheModule.get());
    auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", Fn);
    Builder->SetInsertPoint(BB);

    if (auto *RetVal = E->codegen()) {
      Builder->CreateRet(RetVal);
      fprintf(stderr, "Read top-level expression:\n");
      Fn->print(llvm::errs());
      fprintf(stderr, "\n");
    }
    Fn->eraseFromParent();
  } else {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
  }
}
```

### 3. Update the `main()` Banner

Update the two `printf` lines in `main()`:

```cpp
  printf("Kaleidoscope LLVM IR Generator (Step 4.2: Binary Math IR)\n");
  printf("Type arithmetic expressions followed by ';' and press Enter (Ctrl+D "
         "to exit):\n\n");
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

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void HandleTopLevelExpression() {
  if (auto E = ParseExpression()) {
    auto *FT =
        llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), false);
    auto *Fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                      "__anon_expr", TheModule.get());
    auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", Fn);
    Builder->SetInsertPoint(BB);

    if (auto *RetVal = E->codegen()) {
      Builder->CreateRet(RetVal);
      fprintf(stderr, "Read top-level expression:\n");
      Fn->print(llvm::errs());
      fprintf(stderr, "\n");
    }
    Fn->eraseFromParent();
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

  printf("Kaleidoscope LLVM IR Generator (Step 4.2: Binary Math IR)\n");
  printf("Type arithmetic expressions followed by ';' and press Enter (Ctrl+D "
         "to exit):\n\n");

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

Run `./kaleidoscope` and type `1 + 2 * 3;` followed by `3 < 5;`:

```bash
./kaleidoscope
```

```text
Kaleidoscope LLVM IR Generator (Step 4.2: Binary Math IR)
Type arithmetic expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
Read top-level expression:
define double @__anon_expr() {
entry:
  ret double 7.000000e+00
}

ready> 3 < 5;
Read top-level expression:
define double @__anon_expr() {
entry:
  ret double 1.000000e+00
}

ready>
```

Wait a second: where did `fadd`, `fmul`, `fcmp ult`, and `uitofp` go?!

:::{tip}
**Automatic Constant Folding Inside `IRBuilder`!**
`llvm::IRBuilder<>` includes a built-in `ConstantFolder` by default.  When you call `Builder->CreateFMul(2.0, 3.0, "multmp")`, `IRBuilder` notices that both operands are `ConstantFP` literals, computes `6.0` on the spot at compile time, and returns `ConstantFP(6.0)` without emitting an `fmul` instruction at all!  Then `CreateFAdd(1.0, 6.0, "addtmp")` folds to `7.0` the exact same way, all without configuring a single optimisation pass!

To see real `fmul` and `fadd` instructions appear in the basic block, we need operands whose values are only known at runtime: **function parameters**!
:::

In [Step 4.3](03-functions-ir.md), we will add `def` function definitions and watch LLVM emit real SSA arithmetic instructions on runtime parameters.

---

[Previous: Step 4.1 - LLVM Setup and Constant IR](01-llvm-setup.md) | [Section 4 Overview](index.md) | [Next: Step 4.3 - Function Definitions](03-functions-ir.md)
