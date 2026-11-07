# Step 7.2: Emitting Loop Basic Blocks and PHI Nodes

In C, a loop counter `i` sits in mutable memory and gets overwritten on each pass (`i += step`).  In pure Static Single Assignment (SSA) form, however, virtual registers are immutable.  How do we build a loop counter when no register can ever be reassigned?

You already know the tool from [Step 6.3](../06-control-flow/03-phi-nodes.md): an **SSA PHI node**!

Think about the first instruction of a loop block.  There are two ways control can reach it:

1.  **Initial entry** from the block *before* the loop (`PreheaderBB`), carrying `StartVal`.
2.  **Loop back-edge** from the end of the loop (`LoopEndBB`), carrying `NextVar = Variable + StepVal`.

A `PHINode` at the top of `loop:` selects `StartVal` on the first pass and `NextVar` on every subsequent pass, giving us a pure SSA **loop induction variable** without touching memory at all!

---

## Control Flow Graph for `for ... in`

Here is the cyclic CFG that `ForExprAST::codegen()` constructs:

```text
     +-------------------------------------------------------+
     |               PreheaderBB (current block)             |
     |  %startval = <Start->codegen()>                       |
     |  br label %loop                                       |
     +---------------------------+---------------------------+
                                 |
                                 v
     +-------------------------------------------------------+
     |  loop:                                                |
     |    %i = phi double [ %startval, %PreheaderBB ],       | <---+
     |                    [ %nextvar,  %LoopEndBB ]          |     |
     |    <Body->codegen()>                                  |     |
     |    %stepval  = <Step->codegen() or 1.0>               |     |
     |    %nextvar  = fadd double %i, %stepval               |     |
     |    %endcond  = <End->codegen()>                       |     |
     |    %loopcond = fcmp one double %endcond, 0.000000e+00 |     |
     |    br i1 %loopcond, label %loop, label %afterloop     | ----+
     +---------------------------+---------------------------+  true
                                 |
                                 | false
                                 v
     +-------------------------------------------------------+
     |  afterloop:                                           |
     |    ; restore outer NamedValues[VarName]               |
     |    ; expression evaluates to 0.0                      |
     +-------------------------------------------------------+
```

:::{note}
**Post-Tested Loop Semantics (Counting Iterations Carefully!)**

Look closely at the CFG above and notice two important properties of Kaleidoscope's `for i = start, cond, step in body` loop:

1.  **Post-tested execution**: Control jumps unconditionally from `PreheaderBB` into `loop:` (`br label %loop`), so `Body` always executes at least once before `End` is tested (similar to C's `do { ... } while (cond);`).
2.  **`End` tests the current `%i` before `%nextvar` takes effect**: Inside `loop:`, `NamedValues[VarName]` is bound to `%i` (the `PHINode`).  Even though `%nextvar = fadd double %i, %stepval` is computed before `End->codegen()`, `%i` only advances to `%nextvar` when control branches *back* to the `phi` node at the top of `loop:`.  Consequently, `for i = 1, i < n, 1.0 in body` tests `i < n` on the current `i`: when `i = n - 1`, `i < n` is still true, so the loop runs one more pass with `i = n` and then stops because `n < n` is false.  In total, `for i = 1, i < n, 1.0 in body` executes **`n` times** (`i = 1, 2, ..., n`) for `n >= 1`!
:::

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and replace the stub `ForExprAST::codegen()` with the full SSA loop implementation.

### 1. Entering the Loop and Creating the Induction `PHINode`

First, emit `Start->codegen()` inside the current block (`PreheaderBB`).  Then create `LoopBB`, jump into it, and create the `PHINode` with its first incoming edge (`StartVal` from `PreheaderBB`).  We cannot add the second incoming edge yet because the end of the loop has not been generated!

```cpp
llvm::Value *ForExprAST::codegen() {
  llvm::Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
  llvm::BasicBlock *PreheaderBB = Builder->GetInsertBlock();
  llvm::BasicBlock *LoopBB =
      llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  llvm::PHINode *Variable =
      Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, VarName);
  Variable->addIncoming(StartVal, PreheaderBB);
```

### 2. Lexical Shadowing, Body, Step, and Condition

What if a function `def foo(i)` also contains `for i = 1, i < 5 in ...`?  Inside the loop, `i` should refer to the loop variable, and afterwards `i` should refer back to the parameter `i`!  We save `NamedValues[VarName]` in `OldVal`, bind `NamedValues[VarName] = Variable`, and emit `Body`, `Step`, `NextVar`, and `EndCond`:

```cpp
  llvm::Value *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Variable;

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

  llvm::Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

  llvm::Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  EndCond = Builder->CreateFCmpONE(
      EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)),
      "loopcond");
```

### 3. Closing the Back-Edge and Restoring Scope

Just like `ThenBB = Builder->GetInsertBlock()` in [Step 6.2](../06-control-flow/02-basic-blocks-branching.md), `Body` or `End` might contain nested `if` expressions or inner `for` loops that created extra blocks!  So we capture `LoopEndBB = Builder->GetInsertBlock()` right before emitting the conditional branch back to `LoopBB` or out to `AfterBB`:

```cpp
  llvm::BasicBlock *LoopEndBB = Builder->GetInsertBlock();
  llvm::BasicBlock *AfterBB =
      llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(AfterBB);

  // Wire up the back-edge on Variable, restore NamedValues[VarName], and return 0.0...
```

**Stretch**: Finish the last eight lines of `ForExprAST::codegen()`!

1.  Attach the loop back-edge to the PHI node: `Variable->addIncoming(NextVar, LoopEndBB);`.
2.  Restore outer scope: if `OldVal` is non-null, set `NamedValues[VarName] = OldVal;`, otherwise erase `VarName` with `NamedValues.erase(VarName);`.
3.  Return `0.0` via `llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext))`.

Finally, update the startup banner in `main()`:

```cpp
  printf("Kaleidoscope JIT REPL (Step 7.2: Loop SSA Codegen)\n");
```

---

## Verifying Your Code

Check your `ForExprAST::codegen()` implementation against the complete reference file:

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
  llvm::Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
  llvm::BasicBlock *PreheaderBB = Builder->GetInsertBlock();
  llvm::BasicBlock *LoopBB =
      llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  llvm::PHINode *Variable =
      Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, VarName);
  Variable->addIncoming(StartVal, PreheaderBB);

  llvm::Value *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Variable;

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

  llvm::Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

  llvm::Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  EndCond = Builder->CreateFCmpONE(
      EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)),
      "loopcond");

  llvm::BasicBlock *LoopEndBB = Builder->GetInsertBlock();
  llvm::BasicBlock *AfterBB =
      llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(AfterBB);

  Variable->addIncoming(NextVar, LoopEndBB);

  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
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

  printf("Kaleidoscope JIT REPL (Step 7.2: Loop SSA Codegen)\n");
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

## Compiling and Inspecting the Loop IR

Compile `kaleidoscope.cpp`:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Launch `./kaleidoscope`, define a loop function, and examine the generated SSA back-edge:

```text
Kaleidoscope JIT REPL (Step 7.2: Loop SSA Codegen)
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> def count(n) for i = 1, i < n, 1.0 in i * 2;
Read function definition:
define double @count(double %n) {
entry:
  br label %loop

loop:                                             ; preds = %loop, %entry
  %i = phi double [ 1.000000e+00, %entry ], [ %nextvar, %loop ]
  %multmp = fmul double %i, 2.000000e+00
  %nextvar = fadd double %i, 1.000000e+00
  %cmptmp = fcmp ult double %i, %n
  %booltmp = uitofp i1 %cmptmp to double
  %loopcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %loopcond, label %loop, label %afterloop

afterloop:                                        ; preds = %loop
  ret double 0.000000e+00
}

ready> count(5);
Evaluated to 0.000000
```

Look at `%i = phi double [ 1.000000e+00, %entry ], [ %nextvar, %loop ]`: the loop counter advances cleanly on every iteration without a single mutable memory slot!

Right now `count(5)` runs silently and returns `0.000000`.  In [Step 7.3](03-terminal-graphics.md), you will link two tiny C output helpers into your binary so your JIT loops can print numbers and draw ASCII art directly in your terminal!

---

[Previous: Lexing and Parsing 'for' Loops](01-for-ast.md) | [Section Overview](index.md) | [Next: Terminal Graphics and Host Calls](03-terminal-graphics.md)
