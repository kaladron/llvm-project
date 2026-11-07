# Step 9.2: Local Variables and Assignment

With stack-allocated variables in place from [Step 9.1](01-alloca-stack-variables.md), we are ready to add two pillars of imperative programming to Kaleidoscope:

1.  **The assignment operator `=`**, which mutates an existing variable (`c = a + b`).
2.  **`var ... in` expressions**, which declare one or more scoped local variables with optional initialisers:

```text
def fibi(x)
  var a = 1, b = 1, c in
  (for i = 3, i < x in
     c = a + b :
     a = b :
     b = c) :
  b;
```

Notice that `c` has no `= init` clause, so Kaleidoscope defaults it to `0.0`.

---

## R-Values vs L-Values Without RTTI (`codegenAddress()`)

Think about what `a = b` needs from its two operands:

- On the **right-hand side** (`b`), we want the **value** stored in `b` (an *r-value*, emitted via `RHS->codegen()`, which runs `CreateLoad`).
- On the **left-hand side** (`a`), we do **not** want to load `a`'s old value!  We want `a`'s **memory address** (an *l-value*, its `llvm::AllocaInst *`) so we can pass it to `CreateStore(Val, Variable)`.  And if someone writes nonsense like `1 = 2;`, our compiler should politely reject it (`"destination of '=' must be a variable"`).

:::{note}
**Why Not Use `dynamic_cast<VariableExprAST *>(LHS.get())`?**

LLVM itself is compiled with **`-fno-rtti`** (Run-Time Type Information disabled) to save binary size and vtable overhead, and our `clang++ -fno-rtti` compile command follows the exact same standard!  Under `-fno-rtti`, C++ `dynamic_cast` is illegal and fails to compile.

Instead, we add a clean virtual method `ExprAST::codegenAddress()`!  By default on `ExprAST`, `codegenAddress()` reports `"destination of '=' must be a variable"`.  `VariableExprAST` overrides `codegenAddress()` to return its `AllocaInst *`.  Even better, when you add pointers and struct fields in Part 2 (`*p = 5` or `pt.x = 3`), those AST nodes will simply override `codegenAddress()` too!
:::

```text
+-------------------------------------------------------------------+
|  VarExprAST::codegen()                                            |
|    1. For each (VarName, Init):                                   |
|       - Evaluate InitVal (or 0.0 if omitted) in outer scope       |
|       - Alloca = CreateEntryBlockAlloca(TheFunction, VarName)     |
|       - CreateStore(InitVal, Alloca)                              |
|       - Save NamedValues[VarName] in OldBindings; bind Alloca     |
|    2. BodyVal = Body->codegen()                                   |
|    3. Restore OldBindings (or erase from NamedValues)             |
|    4. Return BodyVal                                              |
+-------------------------------------------------------------------+
```

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply the updates below.

### 1. Lexing the `var` Keyword

Add `tok_var = -109` to `enum Token`:

```cpp
  tok_binary = -107,
  tok_unary = -108,
  tok_var = -109,
```

And match `"var"` inside `gettok()` right after `"unary"`:

```cpp
    if (IdentifierStr == "unary")
      return tok_unary;
    if (IdentifierStr == "var")
      return tok_var;
```

### 2. Adding `codegenAddress()` and `VarExprAST`

Add `virtual llvm::Value *codegenAddress();` to `ExprAST` and override it on `VariableExprAST`:

```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
  virtual llvm::Value *codegenAddress();
};
```

```cpp
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}
  const std::string &getName() const { return Name; }
  llvm::Value *codegen() override;
  llvm::Value *codegenAddress() override;
};
```

Next, add `VarExprAST` right after `ForExprAST`:

```cpp
class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
      std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  llvm::Value *codegen() override;
};
```

### 3. Parsing `var ... in` and Registering `=` Precedence

Add `ParseVarExpr()` right after `ParseForExpr()` to parse a comma-separated list of `id (= expr)?` bindings followed by `in <Body>`:

```cpp
static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken(); // eat 'var'

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  if (CurTok != tok_identifier)
    return LogError("expected identifier after 'var'");

  while (true) {
    std::string Name = IdentifierStr;
    getNextToken(); // eat identifier

    std::unique_ptr<ExprAST> Init;
    if (CurTok == '=') {
      getNextToken(); // eat '='
      Init = ParseExpression();
      if (!Init)
        return nullptr;
    }

    VarNames.emplace_back(std::move(Name), std::move(Init));

    if (CurTok != ',')
      break;
    getNextToken(); // eat ','

    if (CurTok != tok_identifier)
      return LogError("expected identifier list after 'var'");
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' keyword after 'var'");
  getNextToken(); // eat 'in'

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}
```

Hook `tok_var` into `ParsePrimary()`:

```cpp
  case tok_for:
    return ParseForExpr();
  case tok_var:
    return ParseVarExpr();
```

And in `main()`, register `'='` in `BinopPrecedence` at precedence **`2`** right above `BinopPrecedence['<'] = 10;`:

```cpp
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
```

Why `2`?  Because arithmetic (`20`, `40`) and comparisons (`10`) bind tighter than `=` (so `c = a + b` parses as `c = (a + b)`), while our user-defined sequencing operator `:` from [Step 8.3](../08-user-defined-ops/03-binary-operators.md) has precedence `1`, binding *looser* than `=` so `c = a + b : a = b` cleanly sequences two assignments without parentheses!

### 4. Emitting IR for L-Values, Assignment, and `VarExprAST`

First, define `ExprAST::codegenAddress()` right above `NumberExprAST::codegen()`, and refactor `VariableExprAST` so `codegen()` delegates address lookup to `codegenAddress()`:

```cpp
llvm::Value *ExprAST::codegenAddress() {
  return LogErrorV("destination of '=' must be a variable");
}

llvm::Value *NumberExprAST::codegen() {
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegenAddress() {
  llvm::AllocaInst *A = NamedValues[Name];
  if (!A)
    return LogErrorV("unknown variable name");
  return A;
}

llvm::Value *VariableExprAST::codegen() {
  llvm::Value *Addr = codegenAddress();
  if (!Addr)
    return nullptr;
  return Builder->CreateLoad(llvm::Type::getDoubleTy(*TheContext), Addr,
                             Name.c_str());
}
```

Next, handle `Op == '='` right at the top of `BinaryExprAST::codegen()` (before `LHS->codegen()`, because we do not want to emit a dead `load` of the left-hand side!):

```cpp
llvm::Value *BinaryExprAST::codegen() {
  if (Op == '=') {
    llvm::Value *Variable = LHS->codegenAddress();
    if (!Variable)
      return nullptr;
    llvm::Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;
    Builder->CreateStore(Val, Variable);
    return Val;
  }

  llvm::Value *L = LHS->codegen();
```

Finally, add `VarExprAST::codegen()` right after `ForExprAST::codegen()`.  Notice how we evaluate `Init->codegen()` *before* overwriting `NamedValues[VarName]`, so `var x = x + 1 in ...` can initialise an inner `x` using the outer `x`!

```cpp
llvm::Value *VarExprAST::codegen() {
  std::vector<llvm::AllocaInst *> OldBindings;
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();

    llvm::Value *InitVal;
    if (Init) {
      InitVal = Init->codegen();
      if (!InitVal)
        return nullptr;
    } else {
      InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
    }

    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder->CreateStore(InitVal, Alloca);

    OldBindings.push_back(NamedValues[VarName]);
    NamedValues[VarName] = Alloca;
  }

  llvm::Value *BodyVal = Body->codegen();
  if (!BodyVal)
    return nullptr;

  // Restore OldBindings for each VarNames[i].first, then return BodyVal...
```

**Stretch**: Write the restoration loop at the end of `VarExprAST::codegen()`!  Loop `for (unsigned i = 0, e = VarNames.size(); i != e; ++i)`: if `OldBindings[i]` is non-null, restore `NamedValues[VarNames[i].first] = OldBindings[i];`, otherwise remove the local name with `NamedValues.erase(VarNames[i].first);`.  Then `return BodyVal;`.

Update the banner in `main()` as well:

```cpp
  printf("Kaleidoscope JIT REPL (Step 9.2: Local Variables (var/in))\n");
```

---

## Verifying Your Code

Check your `VarExprAST::codegen()` stretch loop and l-value methods against the complete reference file:

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
#include "llvm/IR/Instructions.h"
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
  tok_if = -102,
  tok_then = -103,
  tok_else = -104,
  tok_for = -105,
  tok_in = -106,
  tok_binary = -107,
  tok_unary = -108,
  tok_var = -109,

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
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "in")
      return tok_in;
    if (IdentifierStr == "binary")
      return tok_binary;
    if (IdentifierStr == "unary")
      return tok_unary;
    if (IdentifierStr == "var")
      return tok_var;
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
  virtual llvm::Value *codegenAddress();
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
  llvm::Value *codegenAddress() override;
};

class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}
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

class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}
  llvm::Value *codegen() override;
};

class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(std::string VarName, std::unique_ptr<ExprAST> Start,
             std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
             std::unique_ptr<ExprAST> Body)
      : VarName(std::move(VarName)), Start(std::move(Start)),
        End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}
  llvm::Value *codegen() override;
};

class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
      std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  llvm::Value *codegen() override;
};

class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence;

public:
  PrototypeAST(std::string Name, std::vector<std::string> Args,
               bool IsOperator = false, unsigned Prec = 0)
      : Name(std::move(Name)), Args(std::move(Args)), IsOperator(IsOperator),
        Precedence(Prec) {}

  const std::string &getName() const { return Name; }
  bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
  bool isBinaryOp() const { return IsOperator && Args.size() == 2; }
  char getOperatorName() const { return Name[Name.size() - 1]; }
  unsigned getBinaryPrecedence() const { return Precedence; }
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

static std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken(); // eat 'if'

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return LogError("expected 'then'");
  getNextToken(); // eat 'then'

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return LogError("expected 'else'");
  getNextToken(); // eat 'else'

  auto Else = ParseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                     std::move(Else));
}

static std::unique_ptr<ExprAST> ParseForExpr() {
  getNextToken(); // eat 'for'

  if (CurTok != tok_identifier)
    return LogError("expected identifier after 'for'");

  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  if (CurTok != '=')
    return LogError("expected '=' after 'for'");
  getNextToken(); // eat '='

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ',')
    return LogError("expected ',' after for start value");
  getNextToken(); // eat ','

  auto End = ParseExpression();
  if (!End)
    return nullptr;

  // The step value is optional.
  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken(); // eat ','
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' after 'for'");
  getNextToken(); // eat 'in'

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(std::move(IdName), std::move(Start),
                                      std::move(End), std::move(Step),
                                      std::move(Body));
}

static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken(); // eat 'var'

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  if (CurTok != tok_identifier)
    return LogError("expected identifier after 'var'");

  while (true) {
    std::string Name = IdentifierStr;
    getNextToken(); // eat identifier

    std::unique_ptr<ExprAST> Init;
    if (CurTok == '=') {
      getNextToken(); // eat '='
      Init = ParseExpression();
      if (!Init)
        return nullptr;
    }

    VarNames.emplace_back(std::move(Name), std::move(Init));

    if (CurTok != ',')
      break;
    getNextToken(); // eat ','

    if (CurTok != tok_identifier)
      return LogError("expected identifier list after 'var'");
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' keyword after 'var'");
  getNextToken(); // eat 'in'

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  case tok_if:
    return ParseIfExpr();
  case tok_for:
    return ParseForExpr();
  case tok_var:
    return ParseVarExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}

static std::unique_ptr<ExprAST> ParseUnary() {
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return ParsePrimary();

  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(static_cast<char>(Opc),
                                          std::move(Operand));
  return nullptr;
}

/// binoprhs ::= (binop unary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binary operator

    auto RHS = ParseUnary();
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
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;
  unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  case tok_identifier:
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case tok_unary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("expected unary operator");
    FnName = "unary";
    FnName += static_cast<char>(CurTok);
    Kind = 1;
    getNextToken();
    break;
  case tok_binary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("expected binary operator");
    FnName = "binary";
    FnName += static_cast<char>(CurTok);
    Kind = 2;
    getNextToken();

    if (CurTok == tok_number) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("invalid precedence: must be 1..100");
      BinaryPrecedence = static_cast<unsigned>(NumVal);
      getNextToken();
    }
    break;
  default:
    return LogErrorP("expected function name in prototype");
  }

  if (CurTok != '(')
    return LogErrorP("expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);

  if (CurTok != ')')
    return LogErrorP("expected ')' in prototype");

  getNextToken(); // eat ')'

  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(std::move(FnName), std::move(ArgNames),
                                        Kind != 0, BinaryPrecedence);
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
static std::map<std::string, llvm::AllocaInst *> NamedValues;

static llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction,
                                                llvm::StringRef VarName) {
  llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                         TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr,
                           VarName);
}
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

llvm::Value *ExprAST::codegenAddress() {
  return LogErrorV("destination of '=' must be a variable");
}

llvm::Value *NumberExprAST::codegen() {
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *VariableExprAST::codegenAddress() {
  llvm::AllocaInst *A = NamedValues[Name];
  if (!A)
    return LogErrorV("unknown variable name");
  return A;
}

llvm::Value *VariableExprAST::codegen() {
  llvm::Value *Addr = codegenAddress();
  if (!Addr)
    return nullptr;
  return Builder->CreateLoad(llvm::Type::getDoubleTy(*TheContext), Addr,
                             Name.c_str());
}

llvm::Value *UnaryExprAST::codegen() {
  llvm::Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  llvm::Function *F = getFunction(std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("unknown unary operator");

  return Builder->CreateCall(F, OperandV, "unop");
}

llvm::Value *BinaryExprAST::codegen() {
  if (Op == '=') {
    llvm::Value *Variable = LHS->codegenAddress();
    if (!Variable)
      return nullptr;
    llvm::Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;
    Builder->CreateStore(Val, Variable);
    return Val;
  }

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
    break;
  }

  llvm::Function *F = getFunction(std::string("binary") + Op);
  if (!F)
    return LogErrorV("binary operator not found");

  llvm::Value *Ops[2] = {L, R};
  return Builder->CreateCall(F, Ops, "binop");
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

llvm::Value *IfExprAST::codegen() {
  llvm::Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  CondV = Builder->CreateFCmpONE(
      CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  llvm::BasicBlock *ThenBB =
      llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
  llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit 'then' block.
  Builder->SetInsertPoint(ThenBB);
  llvm::Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();

  // Emit 'else' block.
  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);
  llvm::Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  // Emit 'ifcont' merge block with SSA PHI node.
  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);
  llvm::PHINode *PN =
      Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

llvm::Value *ForExprAST::codegen() {
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

  llvm::Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  Builder->CreateStore(StartVal, Alloca);

  llvm::BasicBlock *LoopBB =
      llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  llvm::AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

  if (!Body->codegen())
    return nullptr;

  llvm::Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
  } else {
    StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
  }

  llvm::Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  llvm::Value *CurVar =
      Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
  llvm::Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
  Builder->CreateStore(NextVar, Alloca);

  EndCond = Builder->CreateFCmpONE(
      EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)),
      "loopcond");

  llvm::BasicBlock *AfterBB =
      llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(AfterBB);

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
}

llvm::Value *VarExprAST::codegen() {
  std::vector<llvm::AllocaInst *> OldBindings;
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();

    llvm::Value *InitVal;
    if (Init) {
      InitVal = Init->codegen();
      if (!InitVal)
        return nullptr;
    } else {
      InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
    }

    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder->CreateStore(InitVal, Alloca);

    OldBindings.push_back(NamedValues[VarName]);
    NamedValues[VarName] = Alloca;
  }

  llvm::Value *BodyVal = Body->codegen();
  if (!BodyVal)
    return nullptr;

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    if (OldBindings[i])
      NamedValues[VarNames[i].first] = OldBindings[i];
    else
      NamedValues.erase(VarNames[i].first);
  }

  return BodyVal;
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

  if (P.isBinaryOp())
    BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

  auto *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    llvm::AllocaInst *Alloca =
        CreateEntryBlockAlloca(TheFunction, Arg.getName());
    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = Alloca;
  }

  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
      TheFunction->eraseFromParent();
      return nullptr;
    }
    return TheFunction;
  }

  TheFunction->eraseFromParent();
  if (P.isBinaryOp())
    BinopPrecedence.erase(P.getOperatorName());
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Host Library Helpers
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc(static_cast<char>(X), stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
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

  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
  InitializeModuleAndManagers();

  printf("Kaleidoscope JIT REPL (Step 9.2: Local Variables (var/in))\n");
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

## Compiling and Inspecting the Unoptimised `alloca` IR

Compile `kaleidoscope.cpp`:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Launch `./kaleidoscope`, define `:`, write an **iterative** Fibonacci function `fibi(x)` using `var` and `=`, and test `1 = 2;` l-value validation:

```text
Kaleidoscope JIT REPL (Step 9.2: Local Variables (var/in))
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> def binary : 1 (x y) y;
Read function definition:
define double @"binary:"(double %x, double %y) {
entry:
  %y2 = alloca double, align 8
  %x1 = alloca double, align 8
  store double %x, ptr %x1, align 8
  store double %y, ptr %y2, align 8
  %y3 = load double, ptr %y2, align 8
  ret double %y3
}

ready> def fibi(x)
  var a = 1, b = 1, c in
  (for i = 3, i < x in
     c = a + b :
     a = b :
     b = c) :
  b;
Read function definition:
define double @fibi(double %x) {
entry:
  %i = alloca double, align 8
  %c = alloca double, align 8
  %b = alloca double, align 8
  %a = alloca double, align 8
  %x1 = alloca double, align 8
  store double %x, ptr %x1, align 8
  store double 1.000000e+00, ptr %a, align 8
  store double 1.000000e+00, ptr %b, align 8
  store double 0.000000e+00, ptr %c, align 8
  store double 3.000000e+00, ptr %i, align 8
  br label %loop

loop:                                             ; preds = %loop, %entry
  %a2 = load double, ptr %a, align 8
  %b3 = load double, ptr %b, align 8
  %addtmp = fadd double %a2, %b3
  store double %addtmp, ptr %c, align 8
  %b4 = load double, ptr %b, align 8
  store double %b4, ptr %a, align 8
  %binop = call double @"binary:"(double %addtmp, double %b4)
  %c5 = load double, ptr %c, align 8
  store double %c5, ptr %b, align 8
  %binop6 = call double @"binary:"(double %binop, double %c5)
  %i7 = load double, ptr %i, align 8
  %x8 = load double, ptr %x1, align 8
  %cmptmp = fcmp ult double %i7, %x8
  %booltmp = uitofp i1 %cmptmp to double
  %i9 = load double, ptr %i, align 8
  %nextvar = fadd double %i9, 1.000000e+00
  store double %nextvar, ptr %i, align 8
  %loopcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %loopcond, label %loop, label %afterloop

afterloop:                                        ; preds = %loop
  %b10 = load double, ptr %b, align 8
  %binop11 = call double @"binary:"(double 0.000000e+00, double %b10)
  ret double %binop11
}

ready> fibi(10);
Evaluated to 55.000000
ready> 1 = 2;
Error: destination of '=' must be a variable
```

`fibi(10)` computes `55.000000`!  Now look closely at the unoptimised IR above so you can see *why* compiler middle-ends exist:

- Even tiny `@"binary:"` allocates two stack slots, stores `%x` and `%y` to memory, and immediately loads `%y2` back out just to return it!
- `@fibi` emits **5 `alloca`s**, **8 `store`s**, and **9 `load`s**, hitting stack memory on every single variable read and write inside the inner loop!

Keep that exact `@fibi` IR in mind.  In [Step 9.3](03-mem2reg-optimisation.md), you will attach LLVM's New Pass Manager with `PromotePass` (`mem2reg`), run this exact same `fibi` definition again, and watch every single `alloca`, `load`, and `store` disappear!

---

[Previous: Stack Allocation with 'alloca'](01-alloca-stack-variables.md) | [Section Overview](index.md) | [Next: Automatic SSA Promotion with 'mem2reg'](03-mem2reg-optimisation.md)
