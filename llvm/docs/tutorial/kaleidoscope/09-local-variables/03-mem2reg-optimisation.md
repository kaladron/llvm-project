# Step 9.3: Automatic SSA Promotion with 'mem2reg'

In [Step 9.2](02-var-in-expressions.md), our `alloca` + `load` + `store` strategy made mutable local variables remarkably easy to emit in the frontend, at the cost of heavy stack traffic in the raw IR.

Now comes the magic half of LLVM's design: **`llvm::PromotePass()`** (universally known by its `opt` pass name, **`mem2reg`**).

`mem2reg` scans all `alloca` instructions in a function's `entry` block.  Whenever a stack slot is only read and written via direct `load` and `store` instructions (its pointer address never escapes), `mem2reg` computes the **dominance frontier** of every store, inserts minimal SSA `phi` nodes at control-flow merge points, rewires all uses to SSA virtual registers, and deletes the `alloca`, `load`, and `store` instructions completely!

```text
+----------------------------------+     +----------------------------------+
|   Frontend Raw IR (Step 9.2)     |     |   LLVM FunctionPassManager       |
|  5x alloca double in entry:      |     |  1. PromotePass    (mem2reg)     |
|  8x store double ..., ptr %slot  | --> |  2. InstCombinePass              |
|  9x load  double, ptr %slot      |     |  3. ReassociatePass              |
+----------------------------------+     |  4. GVNPass                      |
                                         |  5. SimplifyCFGPass              |
                                         +-----------------+----------------+
                                                           |
                                                           v
                                         +----------------------------------+
                                         |   Optimised SSA IR (Step 9.3)    |
                                         |  0x alloca, 0x load, 0x store!   |
                                         |  Pure register phi nodes:        |
                                         |    %a.0, %b.0, %i.0              |
                                         +----------------------------------+
```

:::{note}
**LLVM's New Pass Manager Architecture**

Modern LLVM organises middle-end optimisations around two cooperating families of objects:

- **`FunctionPassManager` (`TheFPM`)**: Holds the ordered pipeline of transformation passes to run on each `llvm::Function`.
- **Analysis Managers (`TheLAM`, `TheFAM`, `TheCGAM`, `TheMAM`)**: Cache on-demand analyses (such as dominator trees, loop info, and alias analysis) at Loop, Function, Call-Graph SCC, and Module granularity, invalidating cached results automatically only when a pass modifies IR.

Remember way back in [Step 5.2](../05-jit/02-jit-engine.md) when we named our module reset helper `InitializeModuleAndManagers()` (evolved from `InitializeModule()` in Section 4)?  Now the `...AndManagers` half of its name finally comes to life!
:::

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and integrate the New Pass Manager pipeline.

### 1. Including Pass Manager and Scalar Transform Headers

Add the New Pass Manager headers and five classic scalar/utility pass headers at the top of `kaleidoscope.cpp`:

```cpp
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
```

### 2. Configuring the Pass and Analysis Managers

Declare the five manager globals right under `ExitOnErr`:

```cpp
static llvm::ExitOnError ExitOnErr;
static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
static std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
static std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
```

Then populate `InitializeModuleAndManagers()` so every fresh module gets a ready-to-run `FunctionPassManager` pipeline:

```cpp
static void InitializeModuleAndManagers() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("KaleidoscopeJIT", *TheContext);
  TheModule->setDataLayout(TheJIT->getDataLayout());
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

  TheFPM = std::make_unique<llvm::FunctionPassManager>();
  TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
  TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
  TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
  TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();

  // Promote allocas to SSA registers (mem2reg) and simplify expressions.
  TheFPM->addPass(llvm::PromotePass());
  TheFPM->addPass(llvm::InstCombinePass());
  TheFPM->addPass(llvm::ReassociatePass());
  TheFPM->addPass(llvm::GVNPass());
  TheFPM->addPass(llvm::SimplifyCFGPass());

  llvm::PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerCGSCCAnalyses(*TheCGAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.registerLoopAnalyses(*TheLAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}
```

Look briefly at what each pass in that 5-pass pipeline contributes:

1.  **`PromotePass` (`mem2reg`)**: Lifts `entry`-block `alloca`s into SSA registers and `phi` nodes.
2.  **`InstCombinePass`**: Peephole-simplifies instruction sequences (for instance, collapsing our `uitofp` + `fcmp one ..., 0.0` pair into a direct boolean branch!).
3.  **`ReassociatePass`**: Canonicalises associative arithmetic (`1 + x + 2` -> `x + (1 + 2)`) so constants group together.
4.  **`GVNPass`** (Global Value Numbering): Eliminates redundant computations that produce identical SSA values.
5.  **`SimplifyCFGPass`**: Folds dead branches and merges redundant basic blocks.

### 3. Running `TheFPM` in `FunctionAST::codegen()`

In `FunctionAST::codegen()`, invoke `TheFPM->run(*TheFunction, *TheFAM);` right after `llvm::verifyFunction` succeeds (always verify your frontend's raw IR *before* handing it to optimisation passes!):

```cpp
  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
      TheFunction->eraseFromParent();
      return nullptr;
    }

    TheFPM->run(*TheFunction, *TheFAM);
    return TheFunction;
  }
```

Finally, update the banner in `main()`:

```cpp
  printf("Kaleidoscope JIT REPL (Step 9.3: SSA Promotion (mem2reg))\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete Section 9 reference implementation:

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
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

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
static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
static std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
static std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;

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

  TheFPM = std::make_unique<llvm::FunctionPassManager>();
  TheLAM = std::make_unique<llvm::LoopAnalysisManager>();
  TheFAM = std::make_unique<llvm::FunctionAnalysisManager>();
  TheCGAM = std::make_unique<llvm::CGSCCAnalysisManager>();
  TheMAM = std::make_unique<llvm::ModuleAnalysisManager>();

  // Promote allocas to SSA registers (mem2reg) and simplify expressions.
  TheFPM->addPass(llvm::PromotePass());
  TheFPM->addPass(llvm::InstCombinePass());
  TheFPM->addPass(llvm::ReassociatePass());
  TheFPM->addPass(llvm::GVNPass());
  TheFPM->addPass(llvm::SimplifyCFGPass());

  llvm::PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerCGSCCAnalyses(*TheCGAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.registerLoopAnalyses(*TheLAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
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

    TheFPM->run(*TheFunction, *TheFAM);
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

  printf("Kaleidoscope JIT REPL (Step 9.3: SSA Promotion (mem2reg))\n");
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

## Compiling and Witnessing `mem2reg` on `fibi(x)`

Compile `kaleidoscope.cpp`, adding **`passes`** to `--libs` so `llvm-config` links `PassBuilder` and the scalar transformation libraries:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Now paste the **exact same** `:` and `fibi(x)` definitions you ran in [Step 9.2](02-var-in-expressions.md) and compare the emitted IR line by line:

```text
Kaleidoscope JIT REPL (Step 9.3: SSA Promotion (mem2reg))
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> def binary : 1 (x y) y;
Read function definition:
define double @"binary:"(double %x, double %y) {
entry:
  ret double %y
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
  br label %loop

loop:                                             ; preds = %loop, %entry
  %a.0 = phi double [ 1.000000e+00, %entry ], [ %b.0, %loop ]
  %b.0 = phi double [ 1.000000e+00, %entry ], [ %addtmp, %loop ]
  %i.0 = phi double [ 3.000000e+00, %entry ], [ %nextvar, %loop ]
  %addtmp = fadd double %a.0, %b.0
  %binop = call double @"binary:"(double %addtmp, double %b.0)
  %binop6 = call double @"binary:"(double %binop, double %addtmp)
  %cmptmp = fcmp ult double %i.0, %x
  %nextvar = fadd double %i.0, 1.000000e+00
  br i1 %cmptmp, label %loop, label %afterloop

afterloop:                                        ; preds = %loop
  %binop11 = call double @"binary:"(double 0.000000e+00, double %addtmp)
  ret double %binop11
}

ready> fibi(10);
Evaluated to 55.000000
```

Take a moment to marvel at what your 5-pass pipeline just did to `@fibi`:

1.  **Zero `alloca`, `load`, or `store` instructions remain**: All five stack allocations from Step 9.2 vanished completely!
2.  **Temporary variable `c` vanished too**: `mem2reg` noticed that `b = c` simply copies `c = a + b`, so `%b.0` takes `%addtmp` directly on the loop back-edge (`%b.0 = phi double [ 1.0, %entry ], [ %addtmp, %loop ]`)!
3.  **Boolean round-trip eliminated**: Remember how Kaleidoscope `<` emits `uitofp i1 %cmptmp to double` and loop codegen emits `fcmp one double %booltmp, 0.0`?  `InstCombinePass` recognised that round-trip and collapsed both instructions into `br i1 %cmptmp, label %loop, label %afterloop`!

---

## Try It Yourself: Imperative Mandelbrot Escape Loop

Remember how `mandelconverger` in [Step 8.4](../08-user-defined-ops/04-mandelbrot.md) needed five recursive parameters because we had no mutable local variables?  Try rewriting `mandelconverge(creal cimag)` in your REPL using `var r = creal, i = cimag, r2 = 0 in ...` and inspect how `PromotePass` turns your imperative variables into sleek SSA `phi` registers!

---

## Next Steps

Your Kaleidoscope language and optimising JIT are feature-complete for Part 1!  Right now, however, Kaleidoscope code only lives in memory while `./kaleidoscope` is running.  In [Section 10: Compiling to Standalone Object Files](../10-object-files/index.md), you will add a `--emit-obj <output.o>` Ahead-Of-Time (AOT) mode directly on top of your compiler so you can emit native `.o` object files and link Kaleidoscope functions into standalone C/C++ executables!

---

[Previous: Local Variables and Assignment](02-var-in-expressions.md) | [Section Overview](index.md) | [Next: Section 10 - Compiling to Standalone Object Files](../10-object-files/index.md)
