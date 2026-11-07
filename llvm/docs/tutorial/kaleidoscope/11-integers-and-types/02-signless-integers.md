# Step 11.2: Signless Registers and Integer Arithmetic

One of the sharpest insights in LLVM's design surprises almost everyone coming from C or Rust: **LLVM integer registers have no signedness**.

There is no `s32` or `u32` type in LLVM IR.  Both `int` (`int32_t`) and `uint` (`uint32_t`) in Kaleidoscope lower to the exact same `i32` register: a raw 32-bit pattern.  Whether those 32 bits mean `-8` or `4,294,967,288` is decided **per instruction**, not per register.

---

## Why Signless Registers Simplify Hardware Lowering

In two's-complement binary arithmetic, addition, subtraction, multiplication, and left-shifting produce the **exact same output bits** whether you interpret the inputs as signed or unsigned!

Consider the 32-bit pattern `0xFFFFFFF8`:

```text
  32-bit Register:  11111111 11111111 11111111 11111000  (0xFFFFFFF8)
                    |
                    +-- Signed reading:    -8
                    +-- Unsigned reading:  4,294,967,288

  Shared Hardware Circuit (Sign-Agnostic):
    add, sub, mul, and, or, xor, shl   -->  One LLVM opcode for both int & uint!

  Sign-Sensitive Hardware Circuits:
    ashr i32 0xFFFFFFF8, 1  -->  0xFFFFFFFC  (-4: copies sign bit 1 into MSB)
    lshr i32 0xFFFFFFF8, 1  -->  0x7FFFFFFC  (2,147,483,644: shifts 0 into MSB)
    sdiv / srem             -->  Signed division & remainder (x86 idiv)
    udiv / urem             -->  Unsigned division & remainder (x86 div)
```

:::{important}
**Frontend `KType` Chooses the Opcode**:
Because `L->getType()` in LLVM IR is merely `i32` for both `int` and `uint`, inspecting the `llvm::Type *` alone cannot tell you whether `/` means `sdiv` or `udiv`.  That is why we attached `KType ExprType` to `ExprAST` in [Step 11.1](01-integer-widths.md): `IntTy.isSigned()` tells `BinaryExprAST::codegen()` which hardware instruction to emit!
:::

---

## Step-by-Step Changes to `kaleidoscope.cpp`

### 1. Lexing Multi-Character Shift Operators (`<<` and `>>`)

Add `tok_shl` and `tok_shr` to `enum Token` right below `tok_arrow`:

```cpp
  // Multi-character punctuation
  tok_arrow = -300,
  tok_shl = -301,
  tok_shr = -302,
};
```

At the bottom of `gettok()`, recognise `<<` and `>>` right after the `->` check:

```cpp
  if (ThisChar == '-' && LastChar == '>') {
    LastChar = getchar();
    return tok_arrow;
  }
  if (ThisChar == '<' && LastChar == '<') {
    LastChar = getchar();
    return tok_shl;
  }
  if (ThisChar == '>' && LastChar == '>') {
    LastChar = getchar();
    return tok_shr;
  }
  return ThisChar;
}
```

### 2. Widening Binary Operator Tokens from `char` to `int`

In Part 1, every binary operator was a single ASCII character (`'+'`, `'<'`, `'='`), so `BinaryExprAST` stored `char Op` and `BinopPrecedence` mapped `char` to `int`.  Now that `tok_shl` (`-301`) and `tok_shr` (`-302`) are negative token IDs, widen both to `int`!

Update `BinaryExprAST`:

```cpp
class BinaryExprAST : public ExprAST {
  int Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  llvm::Value *codegen() override;
};
```

Update `BinopPrecedence` and `GetTokPrecedence()` so negative multi-character token IDs can be looked up directly instead of being rejected by `!isascii(CurTok)`:

```cpp
static std::map<int, int> BinopPrecedence;

static int GetTokPrecedence() {
  auto It = BinopPrecedence.find(CurTok);
  if (It == BinopPrecedence.end() || It->second <= 0)
    return -1;
  return It->second;
}
```

And in `ParseBinOpRHS()`, pass `BinOp` straight into `BinaryExprAST` without casting to `char`:

```cpp
    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
```

### 3. Built-In Unary Negation (`-`) and Logical Not (`!`)

In Part 1, `-x` and `!x` only worked if the user wrote a custom `def unary-(x)` function in Kaleidoscope source.  Now that we have integers, doubles, and booleans, let us give `UnaryExprAST::codegen()` built-in support for `-` and `!` whenever no user-defined `unary` overload matches the operand's `KType`:

```cpp
  std::string FnName = std::string("unary") + Opcode;
  auto PIt = FunctionProtos.find(FnName);
  if (PIt != FunctionProtos.end() &&
      PIt->second->getArgs()[0].second == Operand->getExprType()) {
    llvm::Function *F = getFunction(FnName);
    if (!F)
      return LogErrorV("unknown unary operator");
    ExprType = PIt->second->getReturnType();
    return Builder->CreateCall(F, OperandV, "unop");
  }

  const KType &OpTy = Operand->getExprType();
  if (Opcode == '-') {
    ExprType = OpTy;
    if (OpTy.isInteger())
      return Builder->CreateNeg(OperandV, "negtmp");
    if (OpTy.isFloatingPoint())
      return Builder->CreateFNeg(OperandV, "fnegtmp");
  }
  if (Opcode == '!') {
    ExprType = KType::getBool();
    llvm::Value *Cond = toBoolCond(OperandV, OpTy, "tobool");
    if (!Cond)
      return nullptr;
    return Builder->CreateNot(Cond, "nottmp");
  }
  return LogErrorV("unknown unary operator");
```

:::{tip}
**How LLVM Emits Negation and Bitwise Complement**:
LLVM IR does not even need dedicated `neg` or `not` instructions!  `Builder->CreateNeg(X)` is a convenience helper that emits `sub i32 0, %x`, and `Builder->CreateNot(Cond)` emits `xor i1 %cond, true` (for wider integers, bitwise complement `~x` is simply `x ^ -1`).  Using minimal canonical forms means optimisation passes only have to pattern-match `sub` and `xor`.
:::

### 4. Emitting Integer Arithmetic and Bitwise Operations

Inside `BinaryExprAST::codegen()`, right after the `if (Op == ':')` block and before the `isFloatingPoint()` block, handle integer binary expressions.

**Idea + Stretch**: Notice how `+`, `-`, `*`, `&`, `|`, `^`, and `<<` each map to a single sign-agnostic `IRBuilder` call, while signed `/` and `>>` emit `CreateSDiv` and `CreateAShr`.  Stretch by filling in the unsigned branches (`CreateUDiv` and `CreateLShr`) plus signed and unsigned remainder (`%` via `CreateSRem` and `CreateURem`), then compare against the complete block below:

```cpp
  if (LHS->getExprType().isInteger() && RHS->getExprType().isInteger()) {
    if (LHS->getExprType() != RHS->getExprType())
      return LogErrorV("integer binary operands must have identical types");
    const KType &IntTy = LHS->getExprType();
    ExprType = IntTy;
    switch (Op) {
    case '+':
      return Builder->CreateAdd(L, R, "addtmp");
    case '-':
      return Builder->CreateSub(L, R, "subtmp");
    case '*':
      return Builder->CreateMul(L, R, "multmp");
    case '/':
      return IntTy.isSigned() ? Builder->CreateSDiv(L, R, "sdivtmp")
                              : Builder->CreateUDiv(L, R, "udivtmp");
    case '%':
      return IntTy.isSigned() ? Builder->CreateSRem(L, R, "sremtmp")
                              : Builder->CreateURem(L, R, "uremtmp");
    case '&':
      return Builder->CreateAnd(L, R, "andtmp");
    case '|':
      return Builder->CreateOr(L, R, "ortmp");
    case '^':
      return Builder->CreateXor(L, R, "xortmp");
    case tok_shl:
      return Builder->CreateShl(L, R, "shltmp");
    case tok_shr:
      return IntTy.isSigned() ? Builder->CreateAShr(L, R, "ashrtmp")
                              : Builder->CreateLShr(L, R, "lshrtmp");
    default:
      break;
    }
  }
```

At the bottom of `BinaryExprAST::codegen()`, guard the user-defined `binary<Op>` lookup with `if (Op > 0 && isascii(Op))` so negative multi-character token IDs do not corrupt `FnName`:

```cpp
  if (Op > 0 && isascii(Op)) {
    std::string FnName = std::string("binary") + static_cast<char>(Op);
    auto PIt = FunctionProtos.find(FnName);
    if (PIt != FunctionProtos.end()) {
      if (PIt->second->getArgs()[0].second != LHS->getExprType() ||
          PIt->second->getArgs()[1].second != RHS->getExprType())
        return LogErrorV("operand type mismatch for user binary operator");
      llvm::Function *F = getFunction(FnName);
      ExprType = PIt->second->getReturnType();
      llvm::Value *Ops[2] = {L, R};
      return Builder->CreateCall(F, Ops, "binop");
    }
  }

  return LogErrorV("type mismatch or unsupported binary operator");
```

### 5. Registering Operator Precedences in `main()`

Add C-style precedence levels for bitwise `|`, `^`, `&`, shifts `<<` and `>>`, and remainder `%` in `main()`, and update the startup banner:

```cpp
  BinopPrecedence[':'] = 1;
  BinopPrecedence['='] = 2;
  BinopPrecedence['|'] = 5;
  BinopPrecedence['^'] = 6;
  BinopPrecedence['&'] = 7;
  BinopPrecedence['<'] = 10;
  BinopPrecedence[tok_shl] = 15;
  BinopPrecedence[tok_shr] = 15;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;
  BinopPrecedence['%'] = 40;
```

```cpp
    printf("Kaleidoscope Compiler (Step 11.2: Signless Integer Arithmetic)\n");
```

---

## Full Reference Implementation

Compare your updated `kaleidoscope.cpp` against [code/s11-02-signless-integers.cpp](../code/s11-02-signless-integers.cpp):

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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include <cstring>

#include <cctype>
#include <cstdint>
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
  tok_double = -110,
  tok_int = -111,
  tok_uint = -112,
  tok_i64 = -113,
  tok_byte = -114,
  tok_bool = -115,
  tok_void = -116,
  tok_true = -117,
  tok_false = -118,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
  tok_int_lit = -202,

  // Multi-character punctuation
  tok_arrow = -300,
  tok_shl = -301,
  tok_shr = -302,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number
static int64_t IntVal;            // Filled in if tok_int_lit

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
    if (IdentifierStr == "double")
      return tok_double;
    if (IdentifierStr == "int")
      return tok_int;
    if (IdentifierStr == "uint")
      return tok_uint;
    if (IdentifierStr == "i64")
      return tok_i64;
    if (IdentifierStr == "byte")
      return tok_byte;
    if (IdentifierStr == "bool")
      return tok_bool;
    if (IdentifierStr == "void")
      return tok_void;
    if (IdentifierStr == "true")
      return tok_true;
    if (IdentifierStr == "false")
      return tok_false;
    return tok_identifier;
  }

  // Recognise integer and floating-point literals.
  if (isdigit(LastChar) || LastChar == '.') {
    if (LastChar == '.') {
      int NextChar = getchar();
      if (!isdigit(NextChar)) {
        LastChar = NextChar;
        return '.';
      }
      std::string NumStr = ".";
      LastChar = NextChar;
      while (isdigit(LastChar)) {
        NumStr += static_cast<char>(LastChar);
        LastChar = getchar();
      }
      NumVal = strtod(NumStr.c_str(), nullptr);
      return tok_number;
    }

    std::string NumStr;
    bool IsFloat = false;
    while (isdigit(LastChar)) {
      NumStr += static_cast<char>(LastChar);
      LastChar = getchar();
    }
    if (LastChar == '.') {
      IsFloat = true;
      NumStr += '.';
      LastChar = getchar();
      while (isdigit(LastChar)) {
        NumStr += static_cast<char>(LastChar);
        LastChar = getchar();
      }
    }
    if (IsFloat) {
      NumVal = strtod(NumStr.c_str(), nullptr);
      return tok_number;
    }
    IntVal = strtoll(NumStr.c_str(), nullptr, 10);
    return tok_int_lit;
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
  if (ThisChar == '-' && LastChar == '>') {
    LastChar = getchar();
    return tok_arrow;
  }
  if (ThisChar == '<' && LastChar == '<') {
    LastChar = getchar();
    return tok_shl;
  }
  if (ThisChar == '>' && LastChar == '>') {
    LastChar = getchar();
    return tok_shr;
  }
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Frontend Type System
//===----------------------------------------------------------------------===//

enum class TypeKind {
  Double,
  Int,
  UInt,
  I64,
  Byte,
  Bool,
  Void,
};

struct KType {
  TypeKind Kind = TypeKind::Double;

  static KType getDouble() { return {TypeKind::Double}; }
  static KType getInt() { return {TypeKind::Int}; }
  static KType getUInt() { return {TypeKind::UInt}; }
  static KType getI64() { return {TypeKind::I64}; }
  static KType getByte() { return {TypeKind::Byte}; }
  static KType getBool() { return {TypeKind::Bool}; }
  static KType getVoid() { return {TypeKind::Void}; }

  bool isFloatingPoint() const { return Kind == TypeKind::Double; }
  bool isInteger() const {
    return Kind == TypeKind::Int || Kind == TypeKind::UInt ||
           Kind == TypeKind::I64 || Kind == TypeKind::Byte ||
           Kind == TypeKind::Bool;
  }
  bool isSigned() const {
    return Kind == TypeKind::Int || Kind == TypeKind::I64;
  }
  bool isVoid() const { return Kind == TypeKind::Void; }

  unsigned getBitWidth() const {
    switch (Kind) {
    case TypeKind::Bool:
      return 1;
    case TypeKind::Byte:
      return 8;
    case TypeKind::Int:
    case TypeKind::UInt:
      return 32;
    case TypeKind::I64:
    case TypeKind::Double:
      return 64;
    case TypeKind::Void:
      return 0;
    }
    return 0;
  }

  std::string toString() const {
    switch (Kind) {
    case TypeKind::Double:
      return "double";
    case TypeKind::Int:
      return "int";
    case TypeKind::UInt:
      return "uint";
    case TypeKind::I64:
      return "i64";
    case TypeKind::Byte:
      return "byte";
    case TypeKind::Bool:
      return "bool";
    case TypeKind::Void:
      return "void";
    }
    return "unknown";
  }

  bool operator==(const KType &O) const { return Kind == O.Kind; }
  bool operator!=(const KType &O) const { return !(*this == O); }
};

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
//===----------------------------------------------------------------------===//

class ExprAST {
protected:
  KType ExprType = KType::getDouble();

public:
  virtual ~ExprAST() = default;
  const KType &getExprType() const { return ExprType; }
  virtual llvm::Value *codegen() = 0;
  virtual llvm::Value *codegenAddress();
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}
  llvm::Value *codegen() override;
};

class IntExprAST : public ExprAST {
  int64_t Val;

public:
  explicit IntExprAST(int64_t Val) : Val(Val) {}
  llvm::Value *codegen() override;
};

class BoolExprAST : public ExprAST {
  bool Val;

public:
  explicit BoolExprAST(bool Val) : Val(Val) {}
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
  int Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
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

struct VarDeclItem {
  std::string Name;
  bool HasExplicitType = false;
  KType ExplicitType = KType::getDouble();
  std::unique_ptr<ExprAST> Init;
};

class ForExprAST : public ExprAST {
  std::string VarName;
  bool HasExplicitType;
  KType ExplicitType;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(std::string VarName, bool HasExplicitType, KType ExplicitType,
             std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
             std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> Body)
      : VarName(std::move(VarName)), HasExplicitType(HasExplicitType),
        ExplicitType(std::move(ExplicitType)), Start(std::move(Start)),
        End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}
  llvm::Value *codegen() override;
};

class VarExprAST : public ExprAST {
  std::vector<VarDeclItem> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(std::vector<VarDeclItem> VarNames, std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  llvm::Value *codegen() override;
};

class PrototypeAST {
  std::string Name;
  std::vector<std::pair<std::string, KType>> Args;
  KType RetType;
  bool IsOperator;
  unsigned Precedence;

public:
  PrototypeAST(std::string Name,
               std::vector<std::pair<std::string, KType>> Args,
               KType RetType = KType::getDouble(), bool IsOperator = false,
               unsigned Prec = 0)
      : Name(std::move(Name)), Args(std::move(Args)),
        RetType(std::move(RetType)), IsOperator(IsOperator), Precedence(Prec) {}

  const std::string &getName() const { return Name; }
  const std::vector<std::pair<std::string, KType>> &getArgs() const {
    return Args;
  }
  const KType &getReturnType() const { return RetType; }
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

static std::map<int, int> BinopPrecedence;

static int GetTokPrecedence() {
  auto It = BinopPrecedence.find(CurTok);
  if (It == BinopPrecedence.end() || It->second <= 0)
    return -1;
  return It->second;
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

static bool ParseType(KType &Out) {
  switch (CurTok) {
  case tok_double:
    Out = KType::getDouble();
    getNextToken();
    return true;
  case tok_int:
    Out = KType::getInt();
    getNextToken();
    return true;
  case tok_uint:
    Out = KType::getUInt();
    getNextToken();
    return true;
  case tok_i64:
    Out = KType::getI64();
    getNextToken();
    return true;
  case tok_byte:
    Out = KType::getByte();
    getNextToken();
    return true;
  case tok_bool:
    Out = KType::getBool();
    getNextToken();
    return true;
  case tok_void:
    Out = KType::getVoid();
    getNextToken();
    return true;
  default:
    LogError("expected type name (double, int, uint, i64, byte, bool, void)");
    return false;
  }
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the floating-point number
  return Result;
}

static std::unique_ptr<ExprAST> ParseIntExpr() {
  auto Result = std::make_unique<IntExprAST>(IntVal);
  getNextToken(); // consume the integer literal
  return Result;
}

static std::unique_ptr<ExprAST> ParseBoolExpr() {
  bool Val = (CurTok == tok_true);
  getNextToken(); // consume 'true' or 'false'
  return std::make_unique<BoolExprAST>(Val);
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

  bool HasExplicitType = false;
  KType ExplicitType = KType::getDouble();
  if (CurTok == ':') {
    getNextToken(); // eat ':'
    if (!ParseType(ExplicitType))
      return nullptr;
    HasExplicitType = true;
  }

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

  return std::make_unique<ForExprAST>(
      std::move(IdName), HasExplicitType, std::move(ExplicitType),
      std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken(); // eat 'var'

  std::vector<VarDeclItem> VarNames;

  if (CurTok != tok_identifier)
    return LogError("expected identifier after 'var'");

  while (true) {
    VarDeclItem Item;
    Item.Name = IdentifierStr;
    getNextToken(); // eat identifier

    if (CurTok == ':') {
      getNextToken(); // eat ':'
      if (!ParseType(Item.ExplicitType))
        return nullptr;
      if (Item.ExplicitType.isVoid())
        return LogError("variable cannot have type 'void'");
      Item.HasExplicitType = true;
    }

    if (CurTok == '=') {
      getNextToken(); // eat '='
      Item.Init = ParseExpression();
      if (!Item.Init)
        return nullptr;
    }

    VarNames.push_back(std::move(Item));

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
  case tok_int_lit:
    return ParseIntExpr();
  case tok_true:
  case tok_false:
    return ParseBoolExpr();
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
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',' || CurTok == ';' ||
      CurTok == ')' || CurTok == ']' || CurTok == '}' || CurTok == ':')
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

    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
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

    if (CurTok == tok_int_lit || CurTok == tok_number) {
      double PrecVal =
          (CurTok == tok_int_lit) ? static_cast<double>(IntVal) : NumVal;
      if (PrecVal < 1 || PrecVal > 100)
        return LogErrorP("invalid precedence: must be 1..100");
      BinaryPrecedence = static_cast<unsigned>(PrecVal);
      getNextToken();
    }
    break;
  default:
    return LogErrorP("expected function name in prototype");
  }

  if (CurTok != '(')
    return LogErrorP("expected '(' in prototype");
  getNextToken(); // eat '('

  std::vector<std::pair<std::string, KType>> ArgNames;
  while (CurTok == tok_identifier) {
    std::string ArgName = IdentifierStr;
    getNextToken(); // eat parameter name

    KType ArgTy = KType::getDouble();
    if (CurTok == ':') {
      getNextToken(); // eat ':'
      if (!ParseType(ArgTy))
        return nullptr;
      if (ArgTy.isVoid())
        return LogErrorP("parameter cannot have type 'void'");
    }
    ArgNames.emplace_back(std::move(ArgName), std::move(ArgTy));

    if (CurTok == ',')
      getNextToken(); // optional ',' between parameters
  }

  if (CurTok != ')')
    return LogErrorP("expected ')' in prototype");
  getNextToken(); // eat ')'

  KType RetType = KType::getDouble();
  if (CurTok == tok_arrow) {
    getNextToken(); // eat '->'
    if (!ParseType(RetType))
      return nullptr;
  }

  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(std::move(FnName), std::move(ArgNames),
                                        std::move(RetType), Kind != 0,
                                        BinaryPrecedence);
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

static std::unique_ptr<ExprAST> ParseTopLevelExpr() {
  return ParseExpression();
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
struct VarBinding {
  llvm::AllocaInst *Alloca = nullptr;
  KType Ty = KType::getDouble();
};

static std::map<std::string, VarBinding> NamedValues;

static llvm::Type *toLLVMType(const KType &Ty) {
  switch (Ty.Kind) {
  case TypeKind::Double:
    return llvm::Type::getDoubleTy(*TheContext);
  case TypeKind::Int:
  case TypeKind::UInt:
    return llvm::Type::getInt32Ty(*TheContext);
  case TypeKind::I64:
    return llvm::Type::getInt64Ty(*TheContext);
  case TypeKind::Byte:
    return llvm::Type::getInt8Ty(*TheContext);
  case TypeKind::Bool:
    return llvm::Type::getInt1Ty(*TheContext);
  case TypeKind::Void:
    return llvm::Type::getVoidTy(*TheContext);
  }
  return llvm::Type::getDoubleTy(*TheContext);
}

static llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction,
                                                llvm::Type *VarLLVMTy,
                                                llvm::StringRef VarName) {
  llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                         TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(VarLLVMTy, nullptr, VarName);
}
static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static std::map<std::string, llvm::orc::ResourceTrackerSP> FunctionRTs;
static llvm::ExitOnError ExitOnErr;
static std::unique_ptr<llvm::TargetMachine> TheTargetMachine;
static std::string ObjectOutputPath;
static bool EmitObjectMode = false;
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
  TheModule = std::make_unique<llvm::Module>("Kaleidoscope", *TheContext);
  if (EmitObjectMode) {
    TheModule->setTargetTriple(TheTargetMachine->getTargetTriple());
    TheModule->setDataLayout(TheTargetMachine->createDataLayout());
  } else {
    TheModule->setDataLayout(TheJIT->getDataLayout());
  }
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

static llvm::Value *toBoolCond(llvm::Value *V, const KType &Ty,
                               const char *Name) {
  if (Ty.Kind == TypeKind::Bool)
    return V;
  if (Ty.isInteger())
    return Builder->CreateICmpNE(V, llvm::ConstantInt::get(V->getType(), 0),
                                 Name);
  if (Ty.isFloatingPoint())
    return Builder->CreateFCmpONE(
        V, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), Name);
  return LogErrorV("condition expression must be bool, integer, or double");
}

llvm::Value *NumberExprAST::codegen() {
  ExprType = KType::getDouble();
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *IntExprAST::codegen() {
  if (Val >= INT32_MIN && Val <= INT32_MAX) {
    ExprType = KType::getInt();
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*TheContext),
                                  static_cast<uint64_t>(Val),
                                  /*isSigned=*/true);
  }
  ExprType = KType::getI64();
  return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*TheContext),
                                static_cast<uint64_t>(Val), /*isSigned=*/true);
}

llvm::Value *BoolExprAST::codegen() {
  ExprType = KType::getBool();
  return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*TheContext),
                                Val ? 1 : 0);
}

llvm::Value *VariableExprAST::codegenAddress() {
  auto It = NamedValues.find(Name);
  if (It == NamedValues.end() || !It->second.Alloca)
    return LogErrorV("unknown variable name");
  ExprType = It->second.Ty;
  return It->second.Alloca;
}

llvm::Value *VariableExprAST::codegen() {
  llvm::Value *Addr = codegenAddress();
  if (!Addr)
    return nullptr;
  return Builder->CreateLoad(toLLVMType(ExprType), Addr, Name.c_str());
}

llvm::Value *UnaryExprAST::codegen() {
  llvm::Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  std::string FnName = std::string("unary") + Opcode;
  auto PIt = FunctionProtos.find(FnName);
  if (PIt != FunctionProtos.end() &&
      PIt->second->getArgs()[0].second == Operand->getExprType()) {
    llvm::Function *F = getFunction(FnName);
    if (!F)
      return LogErrorV("unknown unary operator");
    ExprType = PIt->second->getReturnType();
    return Builder->CreateCall(F, OperandV, "unop");
  }

  const KType &OpTy = Operand->getExprType();
  if (Opcode == '-') {
    ExprType = OpTy;
    if (OpTy.isInteger())
      return Builder->CreateNeg(OperandV, "negtmp");
    if (OpTy.isFloatingPoint())
      return Builder->CreateFNeg(OperandV, "fnegtmp");
  }
  if (Opcode == '!') {
    ExprType = KType::getBool();
    llvm::Value *Cond = toBoolCond(OperandV, OpTy, "tobool");
    if (!Cond)
      return nullptr;
    return Builder->CreateNot(Cond, "nottmp");
  }
  return LogErrorV("unknown unary operator");
}

llvm::Value *BinaryExprAST::codegen() {
  if (Op == '=') {
    llvm::Value *Variable = LHS->codegenAddress();
    if (!Variable)
      return nullptr;
    KType DstTy = LHS->getExprType();
    llvm::Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;
    if (DstTy != RHS->getExprType())
      return LogErrorV("type mismatch in '=' assignment");
    Builder->CreateStore(Val, Variable);
    ExprType = DstTy;
    return Val;
  }

  llvm::Value *L = LHS->codegen();
  llvm::Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  if (Op == ':') {
    auto PIt = FunctionProtos.find("binary:");
    if (PIt != FunctionProtos.end() &&
        PIt->second->getArgs()[0].second == LHS->getExprType() &&
        PIt->second->getArgs()[1].second == RHS->getExprType()) {
      llvm::Function *F = getFunction("binary:");
      ExprType = PIt->second->getReturnType();
      llvm::Value *Ops[2] = {L, R};
      return Builder->CreateCall(F, Ops, "binop");
    }
    ExprType = RHS->getExprType();
    return R;
  }

  if (LHS->getExprType().isInteger() && RHS->getExprType().isInteger()) {
    if (LHS->getExprType() != RHS->getExprType())
      return LogErrorV("integer binary operands must have identical types");
    const KType &IntTy = LHS->getExprType();
    ExprType = IntTy;
    switch (Op) {
    case '+':
      return Builder->CreateAdd(L, R, "addtmp");
    case '-':
      return Builder->CreateSub(L, R, "subtmp");
    case '*':
      return Builder->CreateMul(L, R, "multmp");
    case '/':
      return IntTy.isSigned() ? Builder->CreateSDiv(L, R, "sdivtmp")
                              : Builder->CreateUDiv(L, R, "udivtmp");
    case '%':
      return IntTy.isSigned() ? Builder->CreateSRem(L, R, "sremtmp")
                              : Builder->CreateURem(L, R, "uremtmp");
    case '&':
      return Builder->CreateAnd(L, R, "andtmp");
    case '|':
      return Builder->CreateOr(L, R, "ortmp");
    case '^':
      return Builder->CreateXor(L, R, "xortmp");
    case tok_shl:
      return Builder->CreateShl(L, R, "shltmp");
    case tok_shr:
      return IntTy.isSigned() ? Builder->CreateAShr(L, R, "ashrtmp")
                              : Builder->CreateLShr(L, R, "lshrtmp");
    default:
      break;
    }
  }

  if (LHS->getExprType().isFloatingPoint() &&
      RHS->getExprType().isFloatingPoint()) {
    ExprType = KType::getDouble();
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
  }

  if (Op > 0 && isascii(Op)) {
    std::string FnName = std::string("binary") + static_cast<char>(Op);
    auto PIt = FunctionProtos.find(FnName);
    if (PIt != FunctionProtos.end()) {
      if (PIt->second->getArgs()[0].second != LHS->getExprType() ||
          PIt->second->getArgs()[1].second != RHS->getExprType())
        return LogErrorV("operand type mismatch for user binary operator");
      llvm::Function *F = getFunction(FnName);
      ExprType = PIt->second->getReturnType();
      llvm::Value *Ops[2] = {L, R};
      return Builder->CreateCall(F, Ops, "binop");
    }
  }

  return LogErrorV("type mismatch or unsupported binary operator");
}

llvm::Value *CallExprAST::codegen() {
  llvm::Function *CalleeF = getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("unknown function referenced");

  auto PIt = FunctionProtos.find(Callee);
  if (PIt == FunctionProtos.end())
    return LogErrorV("unknown function prototype");
  const PrototypeAST &Proto = *PIt->second;

  if (Proto.getArgs().size() != Args.size())
    return LogErrorV("incorrect # arguments passed");

  std::vector<llvm::Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    llvm::Value *ArgV = Args[i]->codegen();
    if (!ArgV)
      return nullptr;
    if (Args[i]->getExprType() != Proto.getArgs()[i].second)
      return LogErrorV("argument type mismatch in function call");
    ArgsV.push_back(ArgV);
  }

  ExprType = Proto.getReturnType();
  if (ExprType.isVoid())
    return Builder->CreateCall(CalleeF, ArgsV);
  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Value *IfExprAST::codegen() {
  llvm::Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  CondV = toBoolCond(CondV, Cond->getExprType(), "ifcond");
  if (!CondV)
    return nullptr;

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  llvm::BasicBlock *ThenBB =
      llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
  llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  Builder->SetInsertPoint(ThenBB);
  llvm::Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;
  KType ThenTy = Then->getExprType();
  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);
  llvm::Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;
  if (ThenTy != Else->getExprType())
    return LogErrorV("'then' and 'else' branches must have the same type");
  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);
  ExprType = ThenTy;
  if (ThenTy.isVoid())
    return llvm::UndefValue::get(llvm::Type::getVoidTy(*TheContext));

  llvm::PHINode *PN = Builder->CreatePHI(toLLVMType(ThenTy), 2, "iftmp");
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

llvm::Value *ForExprAST::codegen() {
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  llvm::Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  KType LoopTy = HasExplicitType ? ExplicitType : Start->getExprType();
  if (LoopTy != Start->getExprType())
    return LogErrorV("for-loop start value type does not match loop variable");

  llvm::AllocaInst *Alloca =
      CreateEntryBlockAlloca(TheFunction, toLLVMType(LoopTy), VarName);
  Builder->CreateStore(StartVal, Alloca);

  llvm::BasicBlock *LoopBB =
      llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  bool HadOld = NamedValues.count(VarName) > 0;
  VarBinding OldVal = HadOld ? NamedValues[VarName] : VarBinding{};
  NamedValues[VarName] = {Alloca, LoopTy};

  if (!Body->codegen())
    return nullptr;

  llvm::Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
    if (Step->getExprType() != LoopTy)
      return LogErrorV("for-loop step value type does not match loop variable");
  } else if (LoopTy.isInteger()) {
    StepVal = llvm::ConstantInt::get(toLLVMType(LoopTy), 1);
  } else {
    StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
  }

  llvm::Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;
  EndCond = toBoolCond(EndCond, End->getExprType(), "loopcond");
  if (!EndCond)
    return nullptr;

  llvm::Value *CurVar =
      Builder->CreateLoad(Alloca->getAllocatedType(), Alloca, VarName.c_str());
  llvm::Value *NextVar = LoopTy.isInteger()
                             ? Builder->CreateAdd(CurVar, StepVal, "nextvar")
                             : Builder->CreateFAdd(CurVar, StepVal, "nextvar");
  Builder->CreateStore(NextVar, Alloca);

  llvm::BasicBlock *AfterBB =
      llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(AfterBB);

  if (HadOld)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  ExprType = LoopTy;
  return llvm::Constant::getNullValue(toLLVMType(LoopTy));
}

llvm::Value *VarExprAST::codegen() {
  std::vector<std::pair<bool, VarBinding>> OldBindings;
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  for (const auto &Item : VarNames) {
    llvm::Value *InitVal = nullptr;
    KType VarTy = Item.ExplicitType;

    if (Item.Init) {
      InitVal = Item.Init->codegen();
      if (!InitVal)
        return nullptr;
      if (!Item.HasExplicitType)
        VarTy = Item.Init->getExprType();
      else if (VarTy != Item.Init->getExprType())
        return LogErrorV("initialiser type does not match variable type");
    } else {
      InitVal = llvm::Constant::getNullValue(toLLVMType(VarTy));
    }

    llvm::AllocaInst *Alloca =
        CreateEntryBlockAlloca(TheFunction, toLLVMType(VarTy), Item.Name);
    Builder->CreateStore(InitVal, Alloca);

    bool HadOld = NamedValues.count(Item.Name) > 0;
    OldBindings.emplace_back(HadOld,
                             HadOld ? NamedValues[Item.Name] : VarBinding{});
    NamedValues[Item.Name] = {Alloca, VarTy};
  }

  llvm::Value *BodyVal = Body->codegen();
  if (!BodyVal)
    return nullptr;

  for (size_t i = 0; i < VarNames.size(); ++i) {
    if (OldBindings[i].first)
      NamedValues[VarNames[i].Name] = OldBindings[i].second;
    else
      NamedValues.erase(VarNames[i].Name);
  }

  ExprType = Body->getExprType();
  return BodyVal;
}

llvm::Function *PrototypeAST::codegen() {
  std::vector<llvm::Type *> ParamTys;
  ParamTys.reserve(Args.size());
  for (const auto &Arg : Args)
    ParamTys.push_back(toLLVMType(Arg.second));

  auto *FT = llvm::FunctionType::get(toLLVMType(RetType), ParamTys, false);
  auto *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Name,
                                   TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++].first);

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
  unsigned Idx = 0;
  for (auto &Arg : TheFunction->args()) {
    const KType &ArgTy = P.getArgs()[Idx++].second;
    llvm::AllocaInst *Alloca =
        CreateEntryBlockAlloca(TheFunction, toLLVMType(ArgTy), Arg.getName());
    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = {Alloca, ArgTy};
  }

  if (llvm::Value *RetVal = Body->codegen()) {
    if (P.getReturnType().isVoid()) {
      Builder->CreateRetVoid();
    } else {
      if (Body->getExprType() != P.getReturnType()) {
        LogErrorV("function body type does not match return type");
        TheFunction->eraseFromParent();
        return nullptr;
      }
      Builder->CreateRet(RetVal);
    }

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

      if (!EmitObjectMode) {
        if (auto It = FunctionRTs.find(FnName); It != FunctionRTs.end()) {
          ExitOnErr(It->second->remove());
          FunctionRTs.erase(It);
        }

        auto RT = TheJIT->getMainJITDylib().createResourceTracker();
        ExitOnErr(
            TheJIT->addModule(llvm::orc::ThreadSafeModule(
                                  std::move(TheModule), std::move(TheContext)),
                              RT));
        FunctionRTs[FnName] = std::move(RT);
        InitializeModuleAndManagers();
      }
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

static llvm::Function *CompileAnonExpr(ExprAST &Expr, KType &OutType) {
  auto *VoidFT =
      llvm::FunctionType::get(llvm::Type::getVoidTy(*TheContext), false);
  auto *TmpFn = llvm::Function::Create(VoidFT, llvm::Function::PrivateLinkage,
                                       "__anon_tmp", TheModule.get());
  auto *EntryBB = llvm::BasicBlock::Create(*TheContext, "entry", TmpFn);
  Builder->SetInsertPoint(EntryBB);
  NamedValues.clear();

  llvm::Value *RetVal = Expr.codegen();
  if (!RetVal) {
    TmpFn->eraseFromParent();
    return nullptr;
  }

  OutType = Expr.getExprType();
  llvm::Type *RetLLVMTy = OutType.isVoid() ? llvm::Type::getVoidTy(*TheContext)
                                           : toLLVMType(OutType);
  auto *RealFT = llvm::FunctionType::get(RetLLVMTy, false);
  auto *Fn = llvm::Function::Create(RealFT, llvm::Function::ExternalLinkage,
                                    "__anon_expr", TheModule.get());
  Fn->splice(Fn->begin(), TmpFn);
  TmpFn->eraseFromParent();

  if (OutType.isVoid())
    Builder->CreateRetVoid();
  else
    Builder->CreateRet(RetVal);

  if (llvm::verifyFunction(*Fn, &llvm::errs())) {
    Fn->eraseFromParent();
    return nullptr;
  }

  TheFPM->run(*Fn, *TheFAM);
  return Fn;
}

static void HandleTopLevelExpression() {
  auto Expr = ParseTopLevelExpr();
  if (!Expr) {
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
    return;
  }

  KType RetTy;
  llvm::Function *FnIR = CompileAnonExpr(*Expr, RetTy);
  if (!FnIR)
    return;

  if (EmitObjectMode) {
    FnIR->eraseFromParent();
    return;
  }

  auto RT = TheJIT->getMainJITDylib().createResourceTracker();
  auto TSM =
      llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext));
  ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
  InitializeModuleAndManagers();

  if (auto ExprSymbol = TheJIT->lookup("__anon_expr")) {
    auto Addr = ExprSymbol->getAddress();
    switch (RetTy.Kind) {
    case TypeKind::Double:
      fprintf(stderr, "Evaluated to %f\n", Addr.toPtr<double (*)()>()());
      break;
    case TypeKind::Int:
      fprintf(stderr, "Evaluated to %d\n", Addr.toPtr<int32_t (*)()>()());
      break;
    case TypeKind::UInt:
      fprintf(stderr, "Evaluated to %u\n", Addr.toPtr<uint32_t (*)()>()());
      break;
    case TypeKind::I64:
      fprintf(stderr, "Evaluated to %lld\n",
              static_cast<long long>(Addr.toPtr<int64_t (*)()>()()));
      break;
    case TypeKind::Byte:
      fprintf(stderr, "Evaluated to %u\n",
              static_cast<unsigned>(Addr.toPtr<uint8_t (*)()>()()));
      break;
    case TypeKind::Bool:
      fprintf(stderr, "Evaluated to %s\n",
              Addr.toPtr<bool (*)()>()() ? "true" : "false");
      break;
    case TypeKind::Void:
      Addr.toPtr<void (*)()>()();
      break;
    }
  } else {
    llvm::logAllUnhandledErrors(ExprSymbol.takeError(), llvm::errs(),
                                "JIT Error: ");
  }

  ExitOnErr(RT->remove());
}

static void Prompt() {
  if (!EmitObjectMode)
    fprintf(stderr, "ready> ");
}

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

static bool ConfigureTargetMachine() {
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  std::string TargetTripleStr = llvm::sys::getDefaultTargetTriple();
  llvm::Triple TargetTriple(TargetTripleStr);

  std::string Error;
  const llvm::Target *Target =
      llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
  if (!Target) {
    llvm::errs() << "Target lookup error: " << Error << "\n";
    return false;
  }

  llvm::TargetOptions Opt;
  TheTargetMachine.reset(Target->createTargetMachine(
      TargetTriple, "generic", "", Opt, llvm::Reloc::PIC_));
  fprintf(stderr, "Configured TargetMachine for %s\n", TargetTripleStr.c_str());
  return true;
}

static int EmitObjectFile(const std::string &Filename) {
  std::error_code EC;
  llvm::raw_fd_ostream Dest(Filename, EC, llvm::sys::fs::OF_None);
  if (EC) {
    llvm::errs() << "Could not open file: " << EC.message() << "\n";
    return 1;
  }

  llvm::legacy::PassManager CodeGenPasses;
  if (TheTargetMachine->addPassesToEmitFile(
          CodeGenPasses, Dest, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    llvm::errs() << "TargetMachine cannot emit an object file\n";
    return 1;
  }

  CodeGenPasses.run(*TheModule);
  Dest.flush();
  fprintf(stderr, "Wrote %s\n", Filename.c_str());
  return 0;
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--emit-obj") == 0 && i + 1 < argc) {
      EmitObjectMode = true;
      ObjectOutputPath = argv[++i];
    } else {
      fprintf(stderr, "Usage: %s [--emit-obj <output.o>]\n", argv[0]);
      return 1;
    }
  }

  BinopPrecedence[':'] = 1;
  BinopPrecedence['='] = 2;
  BinopPrecedence['|'] = 5;
  BinopPrecedence['^'] = 6;
  BinopPrecedence['&'] = 7;
  BinopPrecedence['<'] = 10;
  BinopPrecedence[tok_shl] = 15;
  BinopPrecedence[tok_shr] = 15;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;
  BinopPrecedence['%'] = 40;

  if (EmitObjectMode) {
    if (!ConfigureTargetMachine())
      return 1;
  } else {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
    printf("Kaleidoscope Compiler (Step 11.2: Signless Integer Arithmetic)\n");
    printf("Type definitions, externs, or expressions followed by ';' (Ctrl+D "
           "to exit):\n\n");
  }

  InitializeModuleAndManagers();

  Prompt();
  getNextToken();
  MainLoop();

  if (EmitObjectMode)
    return EmitObjectFile(ObjectOutputPath);

  return 0;
}
```
</details>

---

## Compiling and Testing in the REPL

Recompile and run `./kaleidoscope`:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
./kaleidoscope
```

Compare the IR generated for `int` versus `uint` division and right-shifts, then evaluate signed shifts, bitmasks, and boolean negation:

```text
Kaleidoscope Compiler (Step 11.2: Signless Integer Arithmetic)
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> def sdiv_demo(a: int, b: int) -> int a / b;
Read function definition:
define i32 @sdiv_demo(i32 %a, i32 %b) {
entry:
  %sdivtmp = sdiv i32 %a, %b
  ret i32 %sdivtmp
}

ready> def udiv_demo(a: uint, b: uint) -> uint a / b;
Read function definition:
define i32 @udiv_demo(i32 %a, i32 %b) {
entry:
  %udivtmp = udiv i32 %a, %b
  ret i32 %udivtmp
}

ready> def sar_demo(x: int, n: int) -> int x >> n;
Read function definition:
define i32 @sar_demo(i32 %x, i32 %n) {
entry:
  %ashrtmp = ashr i32 %x, %n
  ret i32 %ashrtmp
}

ready> def shr_demo(x: uint, n: uint) -> uint x >> n;
Read function definition:
define i32 @shr_demo(i32 %x, i32 %n) {
entry:
  %lshrtmp = lshr i32 %x, %n
  ret i32 %lshrtmp
}

ready> sar_demo(-8, 1);
Evaluated to -4
ready> def mask_bits(x: int) -> int (x << 2) & 255;
Read function definition:
define i32 @mask_bits(i32 %x) {
entry:
  %shltmp = shl i32 %x, 2
  %andtmp = and i32 %shltmp, 252
  ret i32 %andtmp
}

ready> mask_bits(42);
Evaluated to 168
ready> !false;
Evaluated to true
```

Notice two gems in that transcript:
1.  `@sar_demo` and `@shr_demo` have identical LLVM signatures (`define i32 @...(i32 %x, i32 %n)`), yet `@sar_demo` emits `ashr` (yielding `-4` for `-8 >> 1`) while `@shr_demo` emits `lshr` (which shifts a `0` into bit 31, turning `0xFFFFFFF8 >> 1` into `2147483644`, as we will execute directly once we add `as uint` in [Step 11.4](04-type-conversions.md)).
2.  In `@mask_bits`, LLVM's `InstCombine` pass noticed that `(x << 2)` always has its bottom two bits equal to `0`, so it simplified `& 255` (`0b11111111`) into `& 252` (`0b11111100`) automatically!

---

[Previous: Step 11.1](01-integer-widths.md) | [Section Overview](index.md) | [Next: Step 11.3](03-integer-comparisons.md)
