# Step 11.1: Integer Widths and Primitive Types

Welcome to Part 2!  In Part 1, keeping every value as a `double` let us focus on lexing, operator-precedence parsing, SSA construction, `mem2reg`, ORCv2 JIT compilation, and native object emission without stopping to type-check every expression.  Now we take the exact `kaleidoscope.cpp` you completed in [Step 10.3](../10-object-files/03-linking-standalone-binary.md) and give it a real static type system.

To keep every existing Part 1 snippet working without edits, type annotations in Kaleidoscope 201 are **opt-in**:

* Unannotated parameters and return types (`def add(x y) x + y;`) still default to `double`.
* Annotated declarations (`def choose(flag: bool, a: int, b: int) -> int ...`) unlock 1-bit booleans (`i1`), 8-bit bytes (`i8`), 32-bit signed and unsigned integers (`i32`), 64-bit integers (`i64`), and `void` functions.

---

## Kaleidoscope Types vs. LLVM IR Types

Hardware CPUs separate floating-point values from general-purpose integer registers and distinguish integers by bit-width.  Notice in the mapping table below how two distinct Kaleidoscope types (`int` and `uint`) lower to the **exact same** LLVM IR type (`i32`):

```text
  Kaleidoscope Type    Bit-Width    Signed?    LLVM IR Type    C++ Host Type
  -----------------    ---------    -------    ------------    -------------
  double               64-bit       yes (FP)   double          double
  int                  32-bit       signed     i32             int32_t
  uint                 32-bit       unsigned   i32             uint32_t
  i64                  64-bit       signed     i64             int64_t
  byte                 8-bit        unsigned   i8              uint8_t
  bool                 1-bit        unsigned   i1              bool
  void                 0-bit        n/a        void            void
```

:::{note}
**Arbitrary Bit-Widths in LLVM**:
In LLVM IR, integer types are written `iN` for any bit-width `N` from `1` up to `8,388,607` (`2^23 - 1`).  Hardware description compilers and shader backends routinely emit types such as `i7` or `i24`, leaving the target backend to legalise unusual widths into physical CPU registers automatically.
:::

---

## Step-by-Step Changes to `kaleidoscope.cpp`

Open your `kaleidoscope.cpp` from [Step 10.3](../10-object-files/03-linking-standalone-binary.md) and apply the upgrades below.

### 1. Lexing Type Keywords, Booleans, Integer Literals, and `->`

First, include `<cstdint>` alongside `<cctype>` so fixed-width host integer types (`int32_t`, `uint32_t`, `int64_t`, `uint8_t`, `INT32_MIN`, `INT32_MAX`) are available:

```cpp
#include <cctype>
#include <cstdint>
#include <cstdio>
```

Next, extend `enum Token` with tokens for our seven primitive type names, boolean literals `true` and `false`, integer literals (`42` versus `42.0`), and the return-type arrow `->`, and add `IntVal` right under `NumVal`:

```cpp
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
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number
static int64_t IntVal;            // Filled in if tok_int_lit
```

Inside `gettok()`, recognise the new keywords right after `if (IdentifierStr == "var")`:

```cpp
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
```

In Part 1, our numeric lexer lumped every run of `[0-9.]+` into `strtod` and returned `tok_number`.  Now `42` must become an integer literal (`tok_int_lit`) while `42.0` and `.5` remain floating-point literals (`tok_number`).  We also make a bare `.` that is not followed by a digit return `'.'` cleanly (preparing for `...` in Section 12 and struct `.field` access in Section 14).  Replace the numeric literal block in `gettok()` with:

```cpp
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
```

At the very bottom of `gettok()`, check for the two-character `->` sequence right before returning `ThisChar`:

```cpp
  int ThisChar = LastChar;
  LastChar = getchar();
  if (ThisChar == '-' && LastChar == '>') {
    LastChar = getchar();
    return tok_arrow;
  }
  return ThisChar;
}
```

### 2. Defining the Frontend `KType` Descriptor

Why not store `llvm::Type *` directly on AST nodes?  Because LLVM's `i32` does not remember whether the source program wrote `int` (signed) or `uint` (unsigned)!  Our frontend needs its own lightweight type representation (`KType`) to track signedness and source-level semantics before lowering to LLVM IR.

Insert this new section right above the `ExprAST` class hierarchy:

```cpp
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
```

### 3. Carrying `KType` Across the AST

Equip `ExprAST` with a `protected` `ExprType` field (defaulting to `double`) that each node populates during `codegen()`, and add `IntExprAST` and `BoolExprAST` right after `NumberExprAST`:

```cpp
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
```

Local variables in `var` and `for` expressions can now carry an optional explicit type (`var total: int = 0` or `for i: int = 0, i < 10 in ...`), or infer their type from their initialiser when `: <type>` is omitted.  Replace `ForExprAST`, `VarExprAST`, and `PrototypeAST` with their typed counterparts:

```cpp
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
```

### 4. Parsing Type Annotations and Literals

Let us add `ParseType(KType &Out)` right after the `static std::unique_ptr<ExprAST> ParseExpression();` forward declaration.

**Idea + Stretch**: Here is how `ParseType` matches `tok_double` and `tok_int` and advances the token stream.  Try stretching to fill in the remaining five `TypeKind` branches (`tok_uint`, `tok_i64`, `tok_byte`, `tok_bool`, `tok_void`) before checking the block below:

```cpp
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
```

Right below `ParseNumberExpr()`, add `ParseIntExpr()` and `ParseBoolExpr()`, and wire `tok_int_lit`, `tok_true`, and `tok_false` into `ParsePrimary()`:

```cpp
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
```

```cpp
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
```

In `ParseForExpr()` and `ParseVarExpr()`, check `if (CurTok == ':')` after reading the variable identifier to parse an optional type annotation:

```cpp
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
```

And at the bottom of `ParseForExpr()`, pass `HasExplicitType` and `std::move(ExplicitType)` into `ForExprAST`:

```cpp
  return std::make_unique<ForExprAST>(
      std::move(IdName), HasExplicitType, std::move(ExplicitType),
      std::move(Start), std::move(End), std::move(Step), std::move(Body));
```

Update `ParseVarExpr()` similarly so each comma-separated item populates a `VarDeclItem` (rejecting `void` variables early):

```cpp
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
```

In `ParseUnary()`, prevent delimiter characters (`';'`, `')'`, `']'`, `'}'`, `':'`) from being mistaken for user-defined unary operators:

```cpp
static std::unique_ptr<ExprAST> ParseUnary() {
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',' || CurTok == ';' ||
      CurTok == ')' || CurTok == ']' || CurTok == '}' || CurTok == ':')
    return ParsePrimary();
```

Now update `ParsePrototype()`.  Notice three thoughtful touches:
1.  Operator precedence in `def binary | 5 (x y)` is now lexed as `tok_int_lit` (`5` has no decimal point!), so we accept either `tok_int_lit` or `tok_number`.
2.  Commas between parameters (`def add(a: int, b: int)`) are accepted optionally, so Part 1 space-separated parameter lists (`def add(a b)`) remain valid too.
3.  An optional `-> <type>` suffix sets the return type, defaulting to `double`.

```cpp
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
```

Finally, simplify `ParseTopLevelExpr()` so it returns the raw `std::unique_ptr<ExprAST>` directly instead of prematurely wrapping it in a `double`-returning `PrototypeAST`:

```cpp
static std::unique_ptr<ExprAST> ParseTopLevelExpr() {
  return ParseExpression();
}
```

### 5. Lowering `KType` to `llvm::Type *` and Generating Typed IR

In the code generation section, pair each stack `AllocaInst *` in `NamedValues` with its `KType`, add `toLLVMType()`, and parameterise `CreateEntryBlockAlloca()` by the variable's `llvm::Type *` (recalling how `CreateEntryBlockAlloca` hardcoded `double` back in Section 9):

```cpp
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
```

Next, add a helper `toBoolCond()` right after `ExprAST::codegenAddress()`.  Whenever an `if` or `for` condition already has type `bool` (`i1`), it passes straight into `CreateCondBr` with zero extra instructions; integers compare `!= 0` via `CreateICmpNE`, and doubles compare `!= 0.0` via `CreateFCmpONE`:

```cpp
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
```

Now implement `codegen()` for `NumberExprAST`, `IntExprAST`, and `BoolExprAST`.  Notice how `IntExprAST::codegen()` emits a 32-bit `i32` (`int`) whenever the literal fits inside `[INT32_MIN, INT32_MAX]`, and automatically promotes larger literals such as `5000000000` to 64-bit `i64`:

```cpp
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
```

Update `VariableExprAST`, `UnaryExprAST`, `BinaryExprAST`, `CallExprAST`, `IfExprAST`, `ForExprAST`, `VarExprAST`, `PrototypeAST`, and `FunctionAST` so every node checks and propagates `ExprType`:

* **`VariableExprAST`**: Looks up `VarBinding` in `NamedValues`, sets `ExprType = It->second.Ty`, and loads `toLLVMType(ExprType)`.
* **`BinaryExprAST`**: Verifies matching types on `=` assignment, provides `:` as a built-in sequencing operator (`BinopPrecedence[':'] = 1` in `main()`) that works for any operand types (unless overridden by a user `binary:`), and guards floating-point math behind `isFloatingPoint()`.
* **`CallExprAST` and `FunctionAST`**: Handle `void` cleanly!  In LLVM IR, an instruction of `void` type cannot have an SSA register name, so `CallExprAST::codegen()` calls `Builder->CreateCall(CalleeF, ArgsV)` with **no name string** when `ExprType.isVoid()`, and `FunctionAST::codegen()` emits `Builder->CreateRetVoid()`.
* **`IfExprAST`**: Verifies that the `then` and `else` branches produce the same `KType`, skips creating a `PHI` node when both branches are `void`, and otherwise creates `Builder->CreatePHI(toLLVMType(ThenTy), 2, "iftmp")`.

```cpp
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

  std::string FnName = std::string("binary") + Op;
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

  return LogErrorV("unsupported binary operator for operand types (integer "
                   "math arrives in Step 11.2)");
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
```

Update `IfExprAST::codegen()`, `ForExprAST::codegen()`, `VarExprAST::codegen()`, `PrototypeAST::codegen()`, and `FunctionAST::codegen()` as well:

```cpp
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
```

And inside `FunctionAST::codegen()`, replace the parameter `alloca` loop and `Builder->CreateRet(RetVal)`:

```cpp
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
```

### 6. Splicing Polymorphic `__anon_expr` Functions in the JIT

There is one subtle puzzle left: when a user types a top-level expression at the `ready>` prompt, we do not know its `KType` until `Expr.codegen()` finishes emitting basic blocks into an enclosing `llvm::Function`!  Yet in LLVM IR, a `Function`'s return type is immutable once created.

`llvm::Function::splice` solves this cleanly:
1.  We create a temporary private `void` function `__anon_tmp` to host the basic blocks emitted during `Expr.codegen()`.
2.  Once `Expr.getExprType()` is known, we create `__anon_expr` with the **real** return type `RetLLVMTy`, splice every basic block from `__anon_tmp` into `__anon_expr` (`Fn->splice(Fn->begin(), TmpFn)`), and erase `__anon_tmp`.
3.  After JIT compilation, `HandleTopLevelExpression()` switches on `RetTy.Kind`, casts the compiled function address to the matching C++ function pointer (`double (*)()`, `int32_t (*)()`, `uint32_t (*)()`, `int64_t (*)()`, `uint8_t (*)()`, `bool (*)()`, or `void (*)()`), and prints the result!

Replace `HandleTopLevelExpression()` with `CompileAnonExpr()` and the typed dispatcher:

```cpp
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
```

Finally, in `main()`, register `BinopPrecedence[':'] = 1;` right before `BinopPrecedence['='] = 2;` and update the startup banner:

```cpp
  BinopPrecedence[':'] = 1;
  BinopPrecedence['='] = 2;
```

```cpp
    printf("Kaleidoscope Compiler (Step 11.1: Primitive Types & Integer "
           "Widths)\n");
```

---

## Full Reference Implementation

Compare your updated `kaleidoscope.cpp` against the complete snapshot in [code/s11-01-integer-widths.cpp](../code/s11-01-integer-widths.cpp):

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

  std::string FnName = std::string("binary") + Op;
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

  return LogErrorV("unsupported binary operator for operand types (integer "
                   "math arrives in Step 11.2)");
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
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  if (EmitObjectMode) {
    if (!ConfigureTargetMachine())
      return 1;
  } else {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
    printf("Kaleidoscope Compiler (Step 11.1: Primitive Types & Integer "
           "Widths)\n");
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

Compile and launch your compiler:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
./kaleidoscope
```

Verify that Part 1 `double` functions still work unmodified, then test `bool`, `int` (`i32`), and auto-promoted 64-bit (`i64`) integer literals:

```text
Kaleidoscope Compiler (Step 11.1: Primitive Types & Integer Widths)
Type definitions, externs, or expressions followed by ';' (Ctrl+D to exit):

ready> def old_add(x y) x + y;
Read function definition:
define double @old_add(double %x, double %y) {
entry:
  %addtmp = fadd double %x, %y
  ret double %addtmp
}

ready> old_add(1.5, 2.5);
Evaluated to 4.000000
ready> def choose_int(flag: bool, a: int, b: int) -> int if flag then a else b;
Read function definition:
define i32 @choose_int(i1 %flag, i32 %a, i32 %b) {
entry:
  %a.b = select i1 %flag, i32 %a, i32 %b
  ret i32 %a.b
}

ready> choose_int(true, 42, 99);
Evaluated to 42
ready> 5000000000;
Evaluated to 5000000000
ready> false;
Evaluated to false
```

Look closely at `@choose_int`: `flag: bool` lowered directly to `i1 %flag` with zero floating-point comparison instructions, and LLVM's `SimplifyCFG` pass automatically collapsed our `then`/`else`/`ifcont` basic blocks and `PHI` node into a single branchless `select i1 %flag, i32 %a, i32 %b` instruction!

---

[Previous: Step 10.3](../10-object-files/03-linking-standalone-binary.md) | [Section Overview](index.md) | [Next: Step 11.2](02-signless-integers.md)
