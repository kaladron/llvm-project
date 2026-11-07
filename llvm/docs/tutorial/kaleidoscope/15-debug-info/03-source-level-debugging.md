# Step 15.3: Source-Level Debugging with GDB and LLDB

Everything is now in place!  Your compiler tracks line and column coordinates, builds DWARF type and subprogram graphs via `DIBuilder`, preserves stack variable slots when `-g` is enabled, and lowers everything into standard object files via `TargetMachine` (from [Step 10.2](../10-object-files/02-emitting-object-code.md)).

In this step, we will compile a multi-line `demo_debug.ks` program with `-g --emit-obj`, inspect the generated DWARF Debug Information Entries (DIEs) using `llvm-dwarfdump`, link a native binary, and step through Kaleidoscope code line by line in **`gdb`** (with a complete side-by-side **`lldb`** reference table for macOS and Linux LLDB users).

---

## Updating `kaleidoscope.cpp`

The compiler implementation from Step 15.2 is already feature-complete for DWARF emission!  The only code change in `kaleidoscope.cpp` for Step 15.3 is updating the startup banner in `main()`:

```cpp
    printf("Kaleidoscope Compiler (Step 15.3: Source-Level DWARF Debugging)\n");
```

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include "KaleidoscopeJIT.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
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
  tok_as = -119,
  tok_global = -120,
  tok_ptr = -121,
  tok_null = -122,
  tok_sizeof = -123,
  tok_struct = -124,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
  tok_int_lit = -202,
  tok_string = -203,

  // Multi-character punctuation
  tok_arrow = -300,
  tok_shl = -301,
  tok_shr = -302,
  tok_eq = -303,
  tok_ne = -304,
  tok_le = -305,
  tok_ge = -306,
  tok_ellipsis = -307,
};

struct SourceLocation {
  int Line = 1;
  int Col = 0;
};

static SourceLocation CurLoc;
static SourceLocation LexLoc = {1, 0};
static FILE *InputStream = stdin;
static std::string SourceFilePath = "kaleidoscope.ks";

static int advance() {
  int LastChar = fgetc(InputStream);
  if (LastChar == '\n' || LastChar == '\r') {
    LexLoc.Line++;
    LexLoc.Col = 0;
  } else if (LastChar != EOF) {
    LexLoc.Col++;
  }
  return LastChar;
}

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number
static int64_t IntVal;            // Filled in if tok_int_lit
static std::string StringVal;     // Filled in if tok_string

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = advance();

  CurLoc = LexLoc;

  // Recognise identifiers and keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = advance())) || LastChar == '_')
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
    if (IdentifierStr == "as")
      return tok_as;
    if (IdentifierStr == "global")
      return tok_global;
    if (IdentifierStr == "ptr")
      return tok_ptr;
    if (IdentifierStr == "null")
      return tok_null;
    if (IdentifierStr == "sizeof")
      return tok_sizeof;
    if (IdentifierStr == "struct")
      return tok_struct;
    return tok_identifier;
  }

  // Recognise integer and floating-point literals.
  if (isdigit(LastChar) || LastChar == '.') {
    if (LastChar == '.') {
      int NextChar = advance();
      if (NextChar == '.') {
        int ThirdChar = advance();
        if (ThirdChar == '.') {
          LastChar = advance();
          return tok_ellipsis;
        }
        LastChar = ThirdChar;
        return '.';
      }
      if (!isdigit(NextChar)) {
        LastChar = NextChar;
        return '.';
      }
      std::string NumStr = ".";
      LastChar = NextChar;
      while (isdigit(LastChar)) {
        NumStr += static_cast<char>(LastChar);
        LastChar = advance();
      }
      NumVal = strtod(NumStr.c_str(), nullptr);
      return tok_number;
    }

    std::string NumStr;
    bool IsFloat = false;
    while (isdigit(LastChar)) {
      NumStr += static_cast<char>(LastChar);
      LastChar = advance();
    }
    if (LastChar == '.') {
      IsFloat = true;
      NumStr += '.';
      LastChar = advance();
      while (isdigit(LastChar)) {
        NumStr += static_cast<char>(LastChar);
        LastChar = advance();
      }
    }
    if (IsFloat) {
      NumVal = strtod(NumStr.c_str(), nullptr);
      return tok_number;
    }
    IntVal = strtoll(NumStr.c_str(), nullptr, 10);
    return tok_int_lit;
  }

  // Recognise double-quoted string literals with escape sequences.
  if (LastChar == '"') {
    StringVal.clear();
    LastChar = advance(); // eat opening '"'
    while (LastChar != '"' && LastChar != EOF) {
      if (LastChar == '\\') {
        LastChar = advance();
        switch (LastChar) {
        case 'n':
          StringVal += '\n';
          break;
        case 't':
          StringVal += '\t';
          break;
        case 'r':
          StringVal += '\r';
          break;
        case '0':
          StringVal += '\0';
          break;
        case '\\':
          StringVal += '\\';
          break;
        case '"':
          StringVal += '"';
          break;
        default:
          StringVal += static_cast<char>(LastChar);
          break;
        }
      } else {
        StringVal += static_cast<char>(LastChar);
      }
      LastChar = advance();
    }
    if (LastChar == '"')
      LastChar = advance(); // eat closing '"'
    return tok_string;
  }

  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = advance();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = advance();
  if (ThisChar == '-' && LastChar == '>') {
    LastChar = advance();
    return tok_arrow;
  }
  if (ThisChar == '<' && LastChar == '<') {
    LastChar = advance();
    return tok_shl;
  }
  if (ThisChar == '>' && LastChar == '>') {
    LastChar = advance();
    return tok_shr;
  }
  if (ThisChar == '=' && LastChar == '=') {
    LastChar = advance();
    return tok_eq;
  }
  if (ThisChar == '!' && LastChar == '=') {
    LastChar = advance();
    return tok_ne;
  }
  if (ThisChar == '<' && LastChar == '=') {
    LastChar = advance();
    return tok_le;
  }
  if (ThisChar == '>' && LastChar == '=') {
    LastChar = advance();
    return tok_ge;
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
  Pointer,
  Struct,
  Array,
};

struct KType {
  TypeKind Kind = TypeKind::Double;
  std::shared_ptr<const KType> Elem;
  std::string StructName;
  uint64_t ArrayLen = 0;

  static KType getDouble() { return {TypeKind::Double, nullptr, "", 0}; }
  static KType getInt() { return {TypeKind::Int, nullptr, "", 0}; }
  static KType getUInt() { return {TypeKind::UInt, nullptr, "", 0}; }
  static KType getI64() { return {TypeKind::I64, nullptr, "", 0}; }
  static KType getByte() { return {TypeKind::Byte, nullptr, "", 0}; }
  static KType getBool() { return {TypeKind::Bool, nullptr, "", 0}; }
  static KType getVoid() { return {TypeKind::Void, nullptr, "", 0}; }
  static KType getPointer(KType Pointee) {
    return {TypeKind::Pointer,
            std::make_shared<const KType>(std::move(Pointee)), "", 0};
  }
  static KType getStruct(std::string Name) {
    return {TypeKind::Struct, nullptr, std::move(Name), 0};
  }
  static KType getArray(KType ElemTy, uint64_t Len) {
    return {TypeKind::Array, std::make_shared<const KType>(std::move(ElemTy)),
            "", Len};
  }

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
  bool isPointer() const { return Kind == TypeKind::Pointer; }
  bool isStruct() const { return Kind == TypeKind::Struct; }
  bool isArray() const { return Kind == TypeKind::Array; }

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
    case TypeKind::Pointer:
      return 64;
    case TypeKind::Void:
    case TypeKind::Struct:
    case TypeKind::Array:
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
    case TypeKind::Pointer:
      return Elem ? ("ptr<" + Elem->toString() + ">") : "ptr";
    case TypeKind::Struct:
      return StructName;
    case TypeKind::Array:
      return "[" + std::to_string(ArrayLen) + " x " +
             (Elem ? Elem->toString() : "unknown") + "]";
    }
    return "unknown";
  }

  bool operator==(const KType &O) const {
    if (Kind != O.Kind)
      return false;
    if (Kind == TypeKind::Pointer)
      return Elem && O.Elem && *Elem == *O.Elem;
    if (Kind == TypeKind::Struct)
      return StructName == O.StructName;
    if (Kind == TypeKind::Array)
      return ArrayLen == O.ArrayLen && Elem && O.Elem && *Elem == *O.Elem;
    return true;
  }
  bool operator!=(const KType &O) const { return !(*this == O); }
};

struct StructDef {
  std::string Name;
  std::vector<std::pair<std::string, KType>> Fields;
};

static std::map<std::string, StructDef> StructDefs;

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
//===----------------------------------------------------------------------===//

class ExprAST {
protected:
  KType ExprType = KType::getDouble();
  SourceLocation Loc;

public:
  ExprAST(SourceLocation Loc = CurLoc) : Loc(Loc) {}
  virtual ~ExprAST() = default;
  const KType &getExprType() const { return ExprType; }
  int getLine() const { return Loc.Line; }
  int getCol() const { return Loc.Col; }
  virtual llvm::Value *codegen() = 0;
  virtual llvm::Value *codegenAddress();
  virtual bool isAddressable() const { return false; }
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

class StringExprAST : public ExprAST {
  std::string Val;

public:
  explicit StringExprAST(std::string Val) : Val(std::move(Val)) {}
  llvm::Value *codegen() override;
};

class NullPtrExprAST : public ExprAST {
public:
  llvm::Value *codegen() override;
};

class SizeOfExprAST : public ExprAST {
  KType TargetType;

public:
  explicit SizeOfExprAST(KType TargetType)
      : TargetType(std::move(TargetType)) {}
  llvm::Value *codegen() override;
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}
  const std::string &getName() const { return Name; }
  llvm::Value *codegen() override;
  llvm::Value *codegenAddress() override;
  bool isAddressable() const override { return true; }
};

class MemberExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Base;
  std::string FieldName;

public:
  MemberExprAST(std::unique_ptr<ExprAST> Base, std::string FieldName)
      : Base(std::move(Base)), FieldName(std::move(FieldName)) {}
  llvm::Value *codegen() override;
  llvm::Value *codegenAddress() override;
  bool isAddressable() const override { return true; }
};

class IndexExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Base;
  std::unique_ptr<ExprAST> Index;

public:
  IndexExprAST(std::unique_ptr<ExprAST> Base, std::unique_ptr<ExprAST> Index)
      : Base(std::move(Base)), Index(std::move(Index)) {}
  llvm::Value *codegen() override;
  llvm::Value *codegenAddress() override;
  bool isAddressable() const override { return true; }
};

class AddrOfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Operand;

public:
  explicit AddrOfExprAST(std::unique_ptr<ExprAST> Operand)
      : Operand(std::move(Operand)) {}
  llvm::Value *codegen() override;
};

class DerefExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Operand;

public:
  explicit DerefExprAST(std::unique_ptr<ExprAST> Operand)
      : Operand(std::move(Operand)) {}
  llvm::Value *codegen() override;
  llvm::Value *codegenAddress() override;
  bool isAddressable() const override { return true; }
};

class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}
  llvm::Value *codegen() override;
};

class CastExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Operand;
  KType TargetType;

public:
  CastExprAST(std::unique_ptr<ExprAST> Operand, KType TargetType)
      : Operand(std::move(Operand)), TargetType(std::move(TargetType)) {}
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
  SourceLocation Loc;
  std::vector<std::pair<std::string, KType>> Args;
  KType RetType;
  bool IsOperator;
  unsigned Precedence;
  bool IsVarArg;

public:
  PrototypeAST(SourceLocation Loc, std::string Name,
               std::vector<std::pair<std::string, KType>> Args,
               KType RetType = KType::getDouble(), bool IsOperator = false,
               unsigned Prec = 0, bool IsVarArg = false)
      : Name(std::move(Name)), Loc(Loc), Args(std::move(Args)),
        RetType(std::move(RetType)), IsOperator(IsOperator), Precedence(Prec),
        IsVarArg(IsVarArg) {}

  const std::string &getName() const { return Name; }
  int getLine() const { return Loc.Line; }
  const std::vector<std::pair<std::string, KType>> &getArgs() const {
    return Args;
  }
  const KType &getReturnType() const { return RetType; }
  bool isVarArg() const { return IsVarArg; }
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
  case tok_ptr: {
    getNextToken(); // eat 'ptr'
    if (CurTok == '<') {
      getNextToken(); // eat '<'
      KType ElemTy;
      if (!ParseType(ElemTy))
        return false;
      if (ElemTy.isVoid()) {
        LogError("use 'ptr' or 'ptr<byte>' instead of 'ptr<void>'");
        return false;
      }
      if (CurTok != '>') {
        LogError("expected '>' after pointee type in ptr<T>");
        return false;
      }
      getNextToken(); // eat '>'
      Out = KType::getPointer(std::move(ElemTy));
      return true;
    }
    Out = KType::getPointer(KType::getByte());
    return true;
  }
  case '[': {
    getNextToken(); // eat '['
    if (CurTok != tok_int_lit || IntVal <= 0) {
      LogError("expected positive integer length in array type [N x T]");
      return false;
    }
    uint64_t Len = static_cast<uint64_t>(IntVal);
    getNextToken(); // eat length
    if (CurTok != tok_identifier || IdentifierStr != "x") {
      LogError("expected 'x' in array type [N x T]");
      return false;
    }
    getNextToken(); // eat 'x'
    KType ElemTy;
    if (!ParseType(ElemTy) || ElemTy.isVoid()) {
      LogError("invalid element type in [N x T]");
      return false;
    }
    if (CurTok != ']') {
      LogError("expected ']' after array type [N x T]");
      return false;
    }
    getNextToken(); // eat ']'
    Out = KType::getArray(std::move(ElemTy), Len);
    return true;
  }
  case tok_identifier:
    if (StructDefs.count(IdentifierStr)) {
      Out = KType::getStruct(IdentifierStr);
      getNextToken();
      return true;
    }
    LogError("unknown struct type name");
    return false;
  default:
    LogError("expected type name");
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

static std::unique_ptr<ExprAST> ParseStringExpr() {
  auto Result = std::make_unique<StringExprAST>(StringVal);
  getNextToken(); // consume string literal
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
  case tok_string:
    return ParseStringExpr();
  case tok_null:
    getNextToken();
    return std::make_unique<NullPtrExprAST>();
  case tok_sizeof: {
    getNextToken(); // eat 'sizeof'
    if (CurTok != '(')
      return LogError("expected '(' after 'sizeof'");
    getNextToken(); // eat '('
    KType TargetTy;
    if (!ParseType(TargetTy))
      return nullptr;
    if (TargetTy.isVoid())
      return LogError("cannot compute sizeof(void)");
    if (CurTok != ')')
      return LogError("expected ')' after type in 'sizeof'");
    getNextToken(); // eat ')'
    return std::make_unique<SizeOfExprAST>(std::move(TargetTy));
  }
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

static std::unique_ptr<ExprAST> ParsePostfix() {
  auto Expr = ParsePrimary();
  if (!Expr)
    return nullptr;

  while (CurTok == '.' || CurTok == '[') {
    if (CurTok == '.') {
      getNextToken(); // eat '.'
      if (CurTok != tok_identifier)
        return LogError("expected field name after '.'");
      std::string Field = IdentifierStr;
      getNextToken(); // eat field name
      Expr = std::make_unique<MemberExprAST>(std::move(Expr), std::move(Field));
    } else {
      getNextToken(); // eat '['
      auto Idx = ParseExpression();
      if (!Idx)
        return nullptr;
      if (CurTok != ']')
        return LogError("expected ']' after subscript expression");
      getNextToken(); // eat ']'
      Expr = std::make_unique<IndexExprAST>(std::move(Expr), std::move(Idx));
    }
  }
  return Expr;
}

static std::unique_ptr<ExprAST> ParseUnary() {
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',' || CurTok == ';' ||
      CurTok == ')' || CurTok == ']' || CurTok == '}' || CurTok == ':')
    return ParsePostfix();

  if (CurTok == '&') {
    getNextToken(); // eat '&'
    auto Operand = ParseUnary();
    if (!Operand)
      return nullptr;
    return std::make_unique<AddrOfExprAST>(std::move(Operand));
  }
  if (CurTok == '*') {
    getNextToken(); // eat '*'
    auto Operand = ParseUnary();
    if (!Operand)
      return nullptr;
    return std::make_unique<DerefExprAST>(std::move(Operand));
  }

  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(static_cast<char>(Opc),
                                          std::move(Operand));
  return nullptr;
}

/// castexpr ::= unary ('as' type)*
static std::unique_ptr<ExprAST> ParseCast() {
  auto Expr = ParseUnary();
  if (!Expr)
    return nullptr;

  while (CurTok == tok_as) {
    getNextToken(); // eat 'as'
    KType TargetTy;
    if (!ParseType(TargetTy))
      return nullptr;
    if (TargetTy.isVoid())
      return LogError("cannot cast expression to 'void'");
    Expr = std::make_unique<CastExprAST>(std::move(Expr), std::move(TargetTy));
  }
  return Expr;
}

/// binoprhs ::= (binop castexpr)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binary operator

    auto RHS = ParseCast();
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
  auto LHS = ParseCast();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

static std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

static std::unique_ptr<PrototypeAST> ParsePrototype() {
  SourceLocation FnLoc = CurLoc;
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
  bool IsVarArg = false;
  while (CurTok == tok_identifier || CurTok == tok_ellipsis) {
    if (CurTok == tok_ellipsis) {
      IsVarArg = true;
      getNextToken(); // eat '...'
      break;
    }
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

  return std::make_unique<PrototypeAST>(FnLoc, std::move(FnName),
                                        std::move(ArgNames), std::move(RetType),
                                        Kind != 0, BinaryPrecedence, IsVarArg);
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

static llvm::Type *toLLVMType(const KType &Ty);

static llvm::StructType *getOrCreateStructType(const std::string &Name) {
  if (auto *Existing = llvm::StructType::getTypeByName(*TheContext, Name))
    return Existing;

  auto It = StructDefs.find(Name);
  if (It == StructDefs.end())
    return nullptr;

  auto *ST = llvm::StructType::create(*TheContext, Name);
  std::vector<llvm::Type *> FieldTys;
  FieldTys.reserve(It->second.Fields.size());
  for (const auto &F : It->second.Fields)
    FieldTys.push_back(toLLVMType(F.second));
  ST->setBody(FieldTys, /*isPacked=*/false);
  return ST;
}

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
  case TypeKind::Pointer:
    return llvm::PointerType::getUnqual(*TheContext);
  case TypeKind::Struct:
    return getOrCreateStructType(Ty.StructName);
  case TypeKind::Array:
    return llvm::ArrayType::get(toLLVMType(*Ty.Elem), Ty.ArrayLen);
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
static std::map<std::string, KType> GlobalTypes;
static std::map<std::string, llvm::orc::ResourceTrackerSP> GlobalRTs;
static llvm::ExitOnError ExitOnErr;
static std::unique_ptr<llvm::TargetMachine> TheTargetMachine;
static std::string ObjectOutputPath;
static bool EmitObjectMode = false;
static bool DebugInfoMode = false;

struct DebugInfo {
  std::unique_ptr<llvm::DIBuilder> DBuilder;
  llvm::DICompileUnit *TheCU = nullptr;
  llvm::DIFile *UnitFile = nullptr;
  std::vector<llvm::DIScope *> LexicalBlocks;

  void reset() {
    DBuilder.reset();
    TheCU = nullptr;
    UnitFile = nullptr;
    LexicalBlocks.clear();
  }

  llvm::DIType *getDIType(const KType &Ty) {
    switch (Ty.Kind) {
    case TypeKind::Double:
      return DBuilder->createBasicType("double", 64, llvm::dwarf::DW_ATE_float);
    case TypeKind::Int:
      return DBuilder->createBasicType("int", 32, llvm::dwarf::DW_ATE_signed);
    case TypeKind::UInt:
      return DBuilder->createBasicType("uint", 32,
                                       llvm::dwarf::DW_ATE_unsigned);
    case TypeKind::I64:
      return DBuilder->createBasicType("i64", 64, llvm::dwarf::DW_ATE_signed);
    case TypeKind::Byte:
      return DBuilder->createBasicType("byte", 8,
                                       llvm::dwarf::DW_ATE_unsigned_char);
    case TypeKind::Bool:
      return DBuilder->createBasicType("bool", 8, llvm::dwarf::DW_ATE_boolean);
    case TypeKind::Void:
      return nullptr;
    case TypeKind::Pointer:
      return DBuilder->createPointerType(
          Ty.Elem ? getDIType(*Ty.Elem) : nullptr, 64);
    case TypeKind::Struct: {
      auto It = StructDefs.find(Ty.StructName);
      if (It == StructDefs.end())
        return nullptr;
      llvm::StructType *ST = getOrCreateStructType(Ty.StructName);
      const llvm::StructLayout *SL =
          TheModule->getDataLayout().getStructLayout(ST);
      std::vector<llvm::Metadata *> Elements;
      for (unsigned i = 0; i < It->second.Fields.size(); ++i) {
        const auto &Field = It->second.Fields[i];
        llvm::DIType *FieldDITy = getDIType(Field.second);
        uint64_t FieldBits = TheModule->getDataLayout().getTypeAllocSizeInBits(
            toLLVMType(Field.second));
        uint32_t FieldAlignBits = TheModule->getDataLayout()
                                      .getABITypeAlign(toLLVMType(Field.second))
                                      .value() *
                                  8;
        uint64_t OffsetBits = SL->getElementOffsetInBits(i);
        Elements.push_back(DBuilder->createMemberType(
            UnitFile, Field.first, UnitFile, 1, FieldBits, FieldAlignBits,
            OffsetBits, llvm::DINode::FlagZero, FieldDITy));
      }
      return DBuilder->createStructType(
          UnitFile, Ty.StructName, UnitFile, 1, SL->getSizeInBits(),
          SL->getAlignment().value() * 8, llvm::DINode::FlagZero, nullptr,
          DBuilder->getOrCreateArray(Elements));
    }
    case TypeKind::Array: {
      llvm::DIType *ElemDITy = getDIType(*Ty.Elem);
      uint64_t SizeBits =
          TheModule->getDataLayout().getTypeAllocSizeInBits(toLLVMType(Ty));
      uint32_t AlignBits =
          TheModule->getDataLayout().getABITypeAlign(toLLVMType(Ty)).value() *
          8;
      llvm::Metadata *Subscript =
          DBuilder->getOrCreateSubrange(0, static_cast<int64_t>(Ty.ArrayLen));
      return DBuilder->createArrayType(SizeBits, AlignBits, ElemDITy,
                                       DBuilder->getOrCreateArray({Subscript}));
    }
    }
    return nullptr;
  }
  llvm::DISubroutineType *createFunctionType(const PrototypeAST &P) {
    llvm::SmallVector<llvm::Metadata *, 8> EltTys;
    EltTys.push_back(getDIType(P.getReturnType()));
    for (const auto &Arg : P.getArgs())
      EltTys.push_back(getDIType(Arg.second));
    return DBuilder->createSubroutineType(
        DBuilder->getOrCreateTypeArray(EltTys));
  }

  void emitLocation(const ExprAST *AST) {
    if (!DebugInfoMode || !DBuilder)
      return;
    if (!AST) {
      Builder->SetCurrentDebugLocation(llvm::DebugLoc());
      return;
    }
    llvm::DIScope *Scope = LexicalBlocks.empty()
                               ? static_cast<llvm::DIScope *>(TheCU)
                               : LexicalBlocks.back();
    Builder->SetCurrentDebugLocation(llvm::DILocation::get(
        Scope->getContext(), AST->getLine(), AST->getCol(), Scope));
  }
} KSDbgInfo;

static std::unique_ptr<llvm::FunctionPassManager> TheFPM;
static std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
static std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
static std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
static std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;

static llvm::GlobalVariable *getGlobal(const std::string &Name) {
  if (auto *GV = TheModule->getNamedGlobal(Name))
    return GV;
  auto It = GlobalTypes.find(Name);
  if (It == GlobalTypes.end())
    return nullptr;
  return new llvm::GlobalVariable(
      *TheModule, toLLVMType(It->second), /*isConstant=*/false,
      llvm::GlobalValue::ExternalLinkage, /*Initializer=*/nullptr, Name);
}

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

  KSDbgInfo.reset();
  if (DebugInfoMode) {
    TheModule->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                             llvm::DEBUG_METADATA_VERSION);
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 4);
    KSDbgInfo.DBuilder = std::make_unique<llvm::DIBuilder>(*TheModule);
    KSDbgInfo.UnitFile = KSDbgInfo.DBuilder->createFile(SourceFilePath, ".");
    KSDbgInfo.TheCU = KSDbgInfo.DBuilder->createCompileUnit(
        llvm::dwarf::DW_LANG_C, KSDbgInfo.UnitFile, "Kaleidoscope Compiler",
        /*isOptimized=*/false, "", 0);
  }
}

llvm::Value *ExprAST::codegenAddress() {
  return LogErrorV("expression is not an addressable lvalue");
}

static llvm::Value *toBoolCond(llvm::Value *V, const KType &Ty,
                               const char *Name) {
  if (Ty.Kind == TypeKind::Bool)
    return V;
  if (Ty.isInteger())
    return Builder->CreateICmpNE(V, llvm::ConstantInt::get(V->getType(), 0),
                                 Name);
  if (Ty.isPointer())
    return Builder->CreateICmpNE(V,
                                 llvm::ConstantPointerNull::get(
                                     llvm::PointerType::getUnqual(*TheContext)),
                                 Name);
  if (Ty.isFloatingPoint())
    return Builder->CreateFCmpONE(
        V, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), Name);
  return LogErrorV("condition expression must be bool, integer, or double");
}

llvm::Value *NumberExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  ExprType = KType::getDouble();
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
}

llvm::Value *IntExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
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
  KSDbgInfo.emitLocation(this);
  ExprType = KType::getBool();
  return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*TheContext),
                                Val ? 1 : 0);
}

llvm::Value *StringExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  ExprType = KType::getPointer(KType::getByte());
  return Builder->CreateGlobalString(Val, ".str");
}

llvm::Value *NullPtrExprAST::codegen() {
  ExprType = KType::getPointer(KType::getByte());
  return llvm::ConstantPointerNull::get(
      llvm::PointerType::getUnqual(*TheContext));
}

llvm::Value *SizeOfExprAST::codegen() {
  ExprType = KType::getI64();
  uint64_t Bytes =
      TheModule->getDataLayout().getTypeAllocSize(toLLVMType(TargetType));
  return llvm::ConstantInt::get(llvm::Type::getInt64Ty(*TheContext), Bytes);
}

llvm::Value *VariableExprAST::codegenAddress() {
  auto It = NamedValues.find(Name);
  if (It != NamedValues.end() && It->second.Alloca) {
    ExprType = It->second.Ty;
    return It->second.Alloca;
  }
  if (auto *GV = getGlobal(Name)) {
    ExprType = GlobalTypes[Name];
    return GV;
  }
  return LogErrorV("unknown variable name");
}

llvm::Value *VariableExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  llvm::Value *Addr = codegenAddress();
  if (!Addr)
    return nullptr;
  return Builder->CreateLoad(toLLVMType(ExprType), Addr, Name.c_str());
}

llvm::Value *MemberExprAST::codegenAddress() {
  llvm::Value *BasePtr = nullptr;
  std::string StructName;

  if (Base->isAddressable()) {
    BasePtr = Base->codegenAddress();
    if (!BasePtr)
      return nullptr;
    if (Base->getExprType().isStruct()) {
      StructName = Base->getExprType().StructName;
    } else if (Base->getExprType().isPointer() && Base->getExprType().Elem &&
               Base->getExprType().Elem->isStruct()) {
      StructName = Base->getExprType().Elem->StructName;
      BasePtr = Builder->CreateLoad(llvm::PointerType::getUnqual(*TheContext),
                                    BasePtr, "structptr");
    } else {
      return LogErrorV("'.' requires a struct or ptr<Struct> base");
    }
  } else {
    BasePtr = Base->codegen();
    if (!BasePtr)
      return nullptr;
    if (!Base->getExprType().isPointer() || !Base->getExprType().Elem ||
        !Base->getExprType().Elem->isStruct())
      return LogErrorV("'.' requires a struct or ptr<Struct> base");
    StructName = Base->getExprType().Elem->StructName;
  }

  auto It = StructDefs.find(StructName);
  if (It == StructDefs.end())
    return LogErrorV("unknown struct type");

  int FieldIdx = -1;
  for (size_t i = 0; i < It->second.Fields.size(); ++i) {
    if (It->second.Fields[i].first == FieldName) {
      FieldIdx = static_cast<int>(i);
      ExprType = It->second.Fields[i].second;
      break;
    }
  }
  if (FieldIdx < 0)
    return LogErrorV("struct has no such field");

  llvm::StructType *ST = getOrCreateStructType(StructName);
  return Builder->CreateStructGEP(ST, BasePtr, static_cast<unsigned>(FieldIdx),
                                  FieldName + ".addr");
}

llvm::Value *MemberExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  llvm::Value *FieldAddr = codegenAddress();
  if (!FieldAddr)
    return nullptr;
  return Builder->CreateLoad(toLLVMType(ExprType), FieldAddr, FieldName);
}

llvm::Value *IndexExprAST::codegenAddress() {
  llvm::Value *IdxV = Index->codegen();
  if (!IdxV)
    return nullptr;
  if (!Index->getExprType().isInteger())
    return LogErrorV("array/pointer subscript must be an integer");

  llvm::Type *I64Ty = llvm::Type::getInt64Ty(*TheContext);
  if (IdxV->getType() != I64Ty) {
    IdxV = Index->getExprType().isSigned()
               ? Builder->CreateSExt(IdxV, I64Ty, "idx.ext")
               : Builder->CreateZExt(IdxV, I64Ty, "idx.zext");
  }

  if (Base->isAddressable()) {
    llvm::Value *BaseAddr = Base->codegenAddress();
    if (!BaseAddr)
      return nullptr;
    const KType &BTy = Base->getExprType();
    if (BTy.isArray()) {
      ExprType = *BTy.Elem;
      llvm::Value *Zero = llvm::ConstantInt::get(I64Ty, 0);
      return Builder->CreateInBoundsGEP(toLLVMType(BTy), BaseAddr, {Zero, IdxV},
                                        "arrayidx");
    }
    if (BTy.isPointer() && BTy.Elem) {
      llvm::Value *PtrVal = Builder->CreateLoad(
          llvm::PointerType::getUnqual(*TheContext), BaseAddr, "ptrload");
      ExprType = *BTy.Elem;
      return Builder->CreateInBoundsGEP(toLLVMType(ExprType), PtrVal, {IdxV},
                                        "ptridx");
    }
    return LogErrorV("'[]' requires an array or pointer base");
  }

  llvm::Value *BasePtr = Base->codegen();
  if (!BasePtr)
    return nullptr;
  if (!Base->getExprType().isPointer() || !Base->getExprType().Elem)
    return LogErrorV("'[]' requires an array or pointer base");

  ExprType = *Base->getExprType().Elem;
  return Builder->CreateInBoundsGEP(toLLVMType(ExprType), BasePtr, {IdxV},
                                    "ptridx");
}

llvm::Value *IndexExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  llvm::Value *ElemAddr = codegenAddress();
  if (!ElemAddr)
    return nullptr;
  if (ExprType.isArray())
    return ElemAddr;
  return Builder->CreateLoad(toLLVMType(ExprType), ElemAddr, "elem");
}

llvm::Value *AddrOfExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  llvm::Value *Addr = Operand->codegenAddress();
  if (!Addr)
    return nullptr;
  ExprType = KType::getPointer(Operand->getExprType());
  return Addr;
}

llvm::Value *DerefExprAST::codegenAddress() {
  llvm::Value *PtrV = Operand->codegen();
  if (!PtrV)
    return nullptr;
  if (!Operand->getExprType().isPointer() || !Operand->getExprType().Elem)
    return LogErrorV("cannot dereference non-pointer expression");
  ExprType = *Operand->getExprType().Elem;
  return PtrV;
}

llvm::Value *DerefExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  llvm::Value *PtrV = codegenAddress();
  if (!PtrV)
    return nullptr;
  return Builder->CreateLoad(toLLVMType(ExprType), PtrV, "deref");
}

llvm::Value *UnaryExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
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

llvm::Value *CastExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
  llvm::Value *V = Operand->codegen();
  if (!V)
    return nullptr;

  const KType &SrcTy = Operand->getExprType();
  ExprType = TargetType;

  if (SrcTy == TargetType)
    return V;

  if (TargetType.Kind == TypeKind::Bool)
    return toBoolCond(V, SrcTy, "tobool");

  llvm::Type *DstLLVMTy = toLLVMType(TargetType);

  // Integer -> Integer (width change or zero-cost signedness reinterpretation)
  if (SrcTy.isInteger() && TargetType.isInteger()) {
    unsigned SrcBits = SrcTy.getBitWidth();
    unsigned DstBits = TargetType.getBitWidth();
    if (SrcBits == DstBits)
      return V; // e.g. int <-> uint share signless i32!
    if (SrcBits > DstBits)
      return Builder->CreateTrunc(V, DstLLVMTy, "trunctmp");
    return SrcTy.isSigned() ? Builder->CreateSExt(V, DstLLVMTy, "sexttmp")
                            : Builder->CreateZExt(V, DstLLVMTy, "zexttmp");
  }

  // Integer -> Floating-Point
  if (SrcTy.isInteger() && TargetType.isFloatingPoint()) {
    return SrcTy.isSigned() ? Builder->CreateSIToFP(V, DstLLVMTy, "sitofptmp")
                            : Builder->CreateUIToFP(V, DstLLVMTy, "uitofptmp");
  }

  // Floating-Point -> Integer
  if (SrcTy.isFloatingPoint() && TargetType.isInteger()) {
    return TargetType.isSigned()
               ? Builder->CreateFPToSI(V, DstLLVMTy, "fptositmp")
               : Builder->CreateFPToUI(V, DstLLVMTy, "fptouitmp");
  }

  // Opaque pointer <-> pointer retype (zero IR instructions in LLVM!)
  if (SrcTy.isPointer() && TargetType.isPointer())
    return V;
  if (SrcTy.isPointer() && TargetType.Kind == TypeKind::I64)
    return Builder->CreatePtrToInt(V, DstLLVMTy, "ptrtointtmp");
  if (SrcTy.Kind == TypeKind::I64 && TargetType.isPointer())
    return Builder->CreateIntToPtr(V, DstLLVMTy, "inttoptrtmp");

  return LogErrorV("unsupported type conversion");
}

llvm::Value *BinaryExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
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
    case tok_eq:
      ExprType = KType::getBool();
      return Builder->CreateICmpEQ(L, R, "icmptmp");
    case tok_ne:
      ExprType = KType::getBool();
      return Builder->CreateICmpNE(L, R, "icmptmp");
    case '<':
      ExprType = KType::getBool();
      return IntTy.isSigned() ? Builder->CreateICmpSLT(L, R, "icmptmp")
                              : Builder->CreateICmpULT(L, R, "icmptmp");
    case tok_le:
      ExprType = KType::getBool();
      return IntTy.isSigned() ? Builder->CreateICmpSLE(L, R, "icmptmp")
                              : Builder->CreateICmpULE(L, R, "icmptmp");
    case '>':
      ExprType = KType::getBool();
      return IntTy.isSigned() ? Builder->CreateICmpSGT(L, R, "icmptmp")
                              : Builder->CreateICmpUGT(L, R, "icmptmp");
    case tok_ge:
      ExprType = KType::getBool();
      return IntTy.isSigned() ? Builder->CreateICmpSGE(L, R, "icmptmp")
                              : Builder->CreateICmpUGE(L, R, "icmptmp");
    default:
      break;
    }
  }

  if (LHS->getExprType().isPointer() && RHS->getExprType().isPointer()) {
    if (LHS->getExprType() != RHS->getExprType())
      return LogErrorV("pointer comparison requires matching pointer types");
    if (Op == tok_eq) {
      ExprType = KType::getBool();
      return Builder->CreateICmpEQ(L, R, "ptrcmp");
    }
    if (Op == tok_ne) {
      ExprType = KType::getBool();
      return Builder->CreateICmpNE(L, R, "ptrcmp");
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
    case tok_eq:
      ExprType = KType::getBool();
      return Builder->CreateFCmpOEQ(L, R, "fcmptmp");
    case tok_ne:
      ExprType = KType::getBool();
      return Builder->CreateFCmpUNE(L, R, "fcmptmp");
    case '<':
      ExprType = KType::getBool();
      return Builder->CreateFCmpOLT(L, R, "fcmptmp");
    case tok_le:
      ExprType = KType::getBool();
      return Builder->CreateFCmpOLE(L, R, "fcmptmp");
    case '>':
      ExprType = KType::getBool();
      return Builder->CreateFCmpOGT(L, R, "fcmptmp");
    case tok_ge:
      ExprType = KType::getBool();
      return Builder->CreateFCmpOGE(L, R, "fcmptmp");
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
  KSDbgInfo.emitLocation(this);
  llvm::Function *CalleeF = getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("unknown function referenced");

  auto PIt = FunctionProtos.find(Callee);
  if (PIt == FunctionProtos.end())
    return LogErrorV("unknown function prototype");
  const PrototypeAST &Proto = *PIt->second;

  size_t FixedCount = Proto.getArgs().size();
  if (Proto.isVarArg() ? (Args.size() < FixedCount)
                       : (Args.size() != FixedCount))
    return LogErrorV("incorrect # arguments passed");

  std::vector<llvm::Value *> ArgsV;
  for (size_t i = 0, e = Args.size(); i != e; ++i) {
    llvm::Value *ArgV = Args[i]->codegen();
    if (!ArgV)
      return nullptr;
    const KType &ArgTy = Args[i]->getExprType();
    if (i < FixedCount) {
      if (ArgTy != Proto.getArgs()[i].second)
        return LogErrorV("argument type mismatch in function call");
    } else if (ArgTy.isInteger() && ArgTy.getBitWidth() < 32) {
      // C default variadic promotion: i1 / i8 promote to i32.
      ArgV = Builder->CreateZExt(ArgV, llvm::Type::getInt32Ty(*TheContext),
                                 "vararg.prom");
    }
    ArgsV.push_back(ArgV);
  }

  ExprType = Proto.getReturnType();
  if (ExprType.isVoid())
    return Builder->CreateCall(CalleeF, ArgsV);
  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

llvm::Value *IfExprAST::codegen() {
  KSDbgInfo.emitLocation(this);
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
  if (DebugInfoMode && KSDbgInfo.DBuilder && !KSDbgInfo.LexicalBlocks.empty()) {
    llvm::DIScope *Scope = KSDbgInfo.LexicalBlocks.back();
    llvm::DILocalVariable *D = KSDbgInfo.DBuilder->createAutoVariable(
        Scope, VarName, KSDbgInfo.UnitFile, getLine(),
        KSDbgInfo.getDIType(LoopTy), /*AlwaysPreserve=*/true);
    KSDbgInfo.DBuilder->insertDeclare(
        Alloca, D, KSDbgInfo.DBuilder->createExpression(),
        llvm::DILocation::get(Scope->getContext(), getLine(), getCol(), Scope),
        Builder->GetInsertBlock());
  }
  KSDbgInfo.emitLocation(this);
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
    if (DebugInfoMode && KSDbgInfo.DBuilder &&
        !KSDbgInfo.LexicalBlocks.empty()) {
      llvm::DIScope *Scope = KSDbgInfo.LexicalBlocks.back();
      llvm::DILocalVariable *D = KSDbgInfo.DBuilder->createAutoVariable(
          Scope, Item.Name, KSDbgInfo.UnitFile, getLine(),
          KSDbgInfo.getDIType(VarTy), /*AlwaysPreserve=*/true);
      KSDbgInfo.DBuilder->insertDeclare(
          Alloca, D, KSDbgInfo.DBuilder->createExpression(),
          llvm::DILocation::get(Scope->getContext(), getLine(), getCol(),
                                Scope),
          Builder->GetInsertBlock());
    }
    KSDbgInfo.emitLocation(this);
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

  auto *FT = llvm::FunctionType::get(toLLVMType(RetType), ParamTys, IsVarArg);
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

  llvm::DISubprogram *SP = nullptr;
  if (DebugInfoMode && KSDbgInfo.DBuilder) {
    unsigned LineNo = P.getLine();
    SP = KSDbgInfo.DBuilder->createFunction(
        KSDbgInfo.UnitFile, P.getName(), /*LinkageName=*/"", KSDbgInfo.UnitFile,
        LineNo, KSDbgInfo.createFunctionType(P), LineNo,
        llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
    TheFunction->setSubprogram(SP);
    KSDbgInfo.LexicalBlocks.push_back(SP);
    // Keep prologue parameter setup location-less so debuggers stop at Body!
    KSDbgInfo.emitLocation(nullptr);
  }

  NamedValues.clear();
  unsigned Idx = 0;
  for (auto &Arg : TheFunction->args()) {
    const KType &ArgTy = P.getArgs()[Idx].second;
    llvm::AllocaInst *Alloca =
        CreateEntryBlockAlloca(TheFunction, toLLVMType(ArgTy), Arg.getName());

    if (SP) {
      llvm::DILocalVariable *D = KSDbgInfo.DBuilder->createParameterVariable(
          SP, Arg.getName(), Idx + 1, KSDbgInfo.UnitFile, P.getLine(),
          KSDbgInfo.getDIType(ArgTy), /*AlwaysPreserve=*/true);
      KSDbgInfo.DBuilder->insertDeclare(
          Alloca, D, KSDbgInfo.DBuilder->createExpression(),
          llvm::DILocation::get(SP->getContext(), P.getLine(), 0, SP),
          Builder->GetInsertBlock());
    }

    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = {Alloca, ArgTy};
    ++Idx;
  }

  KSDbgInfo.emitLocation(Body.get());

  if (llvm::Value *RetVal = Body->codegen()) {
    if (P.getReturnType().isVoid()) {
      Builder->CreateRetVoid();
    } else {
      if (Body->getExprType() != P.getReturnType()) {
        if (SP)
          KSDbgInfo.LexicalBlocks.pop_back();
        LogErrorV("function body type does not match return type");
        TheFunction->eraseFromParent();
        return nullptr;
      }
      Builder->CreateRet(RetVal);
    }

    if (SP)
      KSDbgInfo.LexicalBlocks.pop_back();

    if (llvm::verifyFunction(*TheFunction, &llvm::errs())) {
      TheFunction->eraseFromParent();
      return nullptr;
    }

    if (!DebugInfoMode)
      TheFPM->run(*TheFunction, *TheFAM);
    return TheFunction;
  }

  if (SP)
    KSDbgInfo.LexicalBlocks.pop_back();
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

        if (DebugInfoMode && KSDbgInfo.DBuilder)
          KSDbgInfo.DBuilder->finalize();
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

static void HandleStruct() {
  getNextToken(); // eat 'struct'
  if (CurTok != tok_identifier) {
    LogError("expected struct name after 'struct'");
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
    return;
  }

  StructDef Def;
  Def.Name = IdentifierStr;
  getNextToken(); // eat struct name

  if (CurTok != '{') {
    LogError("expected '{' in struct definition");
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
    return;
  }
  getNextToken(); // eat '{'

  // Register early so self-referential ptr<Node> fields work!
  StructDefs[Def.Name] = Def;

  while (CurTok != '}' && CurTok != tok_eof) {
    if (CurTok != tok_identifier) {
      LogError("expected field name in struct definition");
      StructDefs.erase(Def.Name);
      while (CurTok != ';' && CurTok != tok_eof)
        getNextToken();
      return;
    }
    std::string FieldName = IdentifierStr;
    getNextToken(); // eat field name

    if (CurTok != ':') {
      LogError("expected ':' after struct field name");
      StructDefs.erase(Def.Name);
      while (CurTok != ';' && CurTok != tok_eof)
        getNextToken();
      return;
    }
    getNextToken(); // eat ':'

    KType FieldTy;
    if (!ParseType(FieldTy) || FieldTy.isVoid() ||
        (FieldTy.isStruct() && FieldTy.StructName == Def.Name)) {
      LogError("invalid struct field type (use ptr<Self> for recursive links)");
      StructDefs.erase(Def.Name);
      while (CurTok != ';' && CurTok != tok_eof)
        getNextToken();
      return;
    }
    Def.Fields.emplace_back(std::move(FieldName), std::move(FieldTy));

    if (CurTok == ',')
      getNextToken();
  }

  if (CurTok != '}') {
    LogError("expected '}' at end of struct definition");
    StructDefs.erase(Def.Name);
    return;
  }
  getNextToken(); // eat '}'

  StructDefs[Def.Name] = Def;
  llvm::StructType *ST = getOrCreateStructType(Def.Name);
  const llvm::StructLayout *SL = TheModule->getDataLayout().getStructLayout(ST);

  fprintf(stderr, "Defined struct %s (size = %llu bytes, align = %u bytes):\n",
          Def.Name.c_str(),
          static_cast<unsigned long long>(SL->getSizeInBytes()),
          static_cast<unsigned>(SL->getAlignment().value()));
  for (unsigned i = 0; i < Def.Fields.size(); ++i) {
    fprintf(stderr, "  +%llu: %s (%s)\n",
            static_cast<unsigned long long>(SL->getElementOffset(i)),
            Def.Fields[i].first.c_str(),
            Def.Fields[i].second.toString().c_str());
  }
}

static void HandleGlobal() {
  getNextToken(); // eat 'global'
  if (CurTok != tok_identifier) {
    LogError("expected identifier after 'global'");
    while (CurTok != ';' && CurTok != tok_eof)
      getNextToken();
    return;
  }

  std::string Name = IdentifierStr;
  getNextToken(); // eat identifier

  KType VarTy = KType::getDouble();
  bool HasType = false;
  if (CurTok == ':') {
    getNextToken(); // eat ':'
    if (!ParseType(VarTy) || VarTy.isVoid()) {
      if (VarTy.isVoid())
        LogError("global variable cannot have type 'void'");
      while (CurTok != ';' && CurTok != tok_eof)
        getNextToken();
      return;
    }
    HasType = true;
  }

  llvm::Constant *InitConst = nullptr;
  if (CurTok == '=') {
    getNextToken(); // eat '='
    bool Negate = false;
    if (CurTok == '-') {
      Negate = true;
      getNextToken();
    }
    if (CurTok == tok_int_lit) {
      int64_t V = Negate ? -IntVal : IntVal;
      if (!HasType)
        VarTy = (V >= INT32_MIN && V <= INT32_MAX) ? KType::getInt()
                                                   : KType::getI64();
      InitConst = llvm::ConstantInt::get(
          toLLVMType(VarTy), static_cast<uint64_t>(V), VarTy.isSigned());
      getNextToken();
    } else if (CurTok == tok_number) {
      double V = Negate ? -NumVal : NumVal;
      if (!HasType)
        VarTy = KType::getDouble();
      InitConst = llvm::ConstantFP::get(*TheContext, llvm::APFloat(V));
      getNextToken();
    } else if (!Negate && (CurTok == tok_true || CurTok == tok_false)) {
      if (!HasType)
        VarTy = KType::getBool();
      InitConst = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*TheContext),
                                         CurTok == tok_true ? 1 : 0);
      getNextToken();
    } else {
      LogError("global initialiser must be a constant literal");
      while (CurTok != ';' && CurTok != tok_eof)
        getNextToken();
      return;
    }
  } else {
    InitConst = llvm::Constant::getNullValue(toLLVMType(VarTy));
  }

  if (EmitObjectMode && TheModule->getNamedGlobal(Name)) {
    LogError("global variable cannot be redefined in object emission mode");
    return;
  }

  auto *GV = new llvm::GlobalVariable(
      *TheModule, toLLVMType(VarTy), /*isConstant=*/false,
      llvm::GlobalValue::ExternalLinkage, InitConst, Name);
  GlobalTypes[Name] = VarTy;

  fprintf(stderr, "Read global variable:\n");
  GV->print(llvm::errs());
  fprintf(stderr, "\n");

  if (!EmitObjectMode) {
    if (auto It = GlobalRTs.find(Name); It != GlobalRTs.end()) {
      ExitOnErr(It->second->remove());
      GlobalRTs.erase(It);
    }
    if (DebugInfoMode && KSDbgInfo.DBuilder)
      KSDbgInfo.DBuilder->finalize();
    auto RT = TheJIT->getMainJITDylib().createResourceTracker();
    ExitOnErr(
        TheJIT->addModule(llvm::orc::ThreadSafeModule(std::move(TheModule),
                                                      std::move(TheContext)),
                          RT));
    GlobalRTs[Name] = std::move(RT);
    InitializeModuleAndManagers();
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
  Builder->SetCurrentDebugLocation(llvm::DebugLoc());
  NamedValues.clear();

  llvm::Value *RetVal = Expr.codegen();
  if (!RetVal) {
    TmpFn->eraseFromParent();
    return nullptr;
  }

  OutType = Expr.getExprType();
  bool DropAggregateRet = OutType.isStruct() || OutType.isArray();
  llvm::Type *RetLLVMTy = (OutType.isVoid() || DropAggregateRet)
                              ? llvm::Type::getVoidTy(*TheContext)
                              : toLLVMType(OutType);
  auto *RealFT = llvm::FunctionType::get(RetLLVMTy, false);
  auto *Fn = llvm::Function::Create(RealFT, llvm::Function::ExternalLinkage,
                                    "__anon_expr", TheModule.get());
  Fn->splice(Fn->begin(), TmpFn);
  TmpFn->eraseFromParent();

  if (OutType.isVoid() || DropAggregateRet)
    Builder->CreateRetVoid();
  else
    Builder->CreateRet(RetVal);

  if (llvm::verifyFunction(*Fn, &llvm::errs())) {
    Fn->eraseFromParent();
    return nullptr;
  }

  if (!DebugInfoMode)
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

  if (DebugInfoMode && KSDbgInfo.DBuilder)
    KSDbgInfo.DBuilder->finalize();
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
    case TypeKind::Pointer:
      fprintf(stderr, "Evaluated to %p\n", Addr.toPtr<void *(*)()>()());
      break;
    case TypeKind::Struct:
      Addr.toPtr<void (*)()>()();
      fprintf(stderr, "Evaluated struct %s\n", RetTy.StructName.c_str());
      break;
    case TypeKind::Array:
      Addr.toPtr<void (*)()>()();
      fprintf(stderr, "Evaluated %s\n", RetTy.toString().c_str());
      break;
    }
  } else {
    llvm::logAllUnhandledErrors(ExprSymbol.takeError(), llvm::errs(),
                                "JIT Error: ");
  }

  ExitOnErr(RT->remove());
}

static void Prompt() {
  if (!EmitObjectMode && InputStream == stdin)
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
    case tok_global:
      HandleGlobal();
      break;
    case tok_struct:
      HandleStruct();
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
    } else if (std::strcmp(argv[i], "-g") == 0 ||
               std::strcmp(argv[i], "--debug") == 0) {
      DebugInfoMode = true;
    } else if (argv[i][0] != '-') {
      SourceFilePath = argv[i];
      InputStream = std::fopen(SourceFilePath.c_str(), "r");
      if (!InputStream) {
        fprintf(stderr, "Could not open input file: %s\n", argv[i]);
        return 1;
      }
    } else {
      fprintf(stderr, "Usage: %s [-g] [<input.ks>] [--emit-obj <output.o>]\n",
              argv[0]);
      return 1;
    }
  }

  BinopPrecedence[':'] = 1;
  BinopPrecedence['='] = 2;
  BinopPrecedence['|'] = 5;
  BinopPrecedence['^'] = 6;
  BinopPrecedence['&'] = 7;
  BinopPrecedence[tok_eq] = 8;
  BinopPrecedence[tok_ne] = 8;
  BinopPrecedence['<'] = 10;
  BinopPrecedence[tok_le] = 10;
  BinopPrecedence['>'] = 10;
  BinopPrecedence[tok_ge] = 10;
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
    printf("Kaleidoscope Compiler (Step 15.3: Source-Level DWARF Debugging)\n");
    printf("Type definitions, externs, or expressions followed by ';' (Ctrl+D "
           "to exit):\n\n");
  }

  InitializeModuleAndManagers();

  Prompt();
  getNextToken();
  MainLoop();

  if (InputStream != stdin)
    std::fclose(InputStream);

  if (EmitObjectMode) {
    if (DebugInfoMode && KSDbgInfo.DBuilder)
      KSDbgInfo.DBuilder->finalize();
    return EmitObjectFile(ObjectOutputPath);
  }

  return 0;
}
```
</details>

Compile your `kaleidoscope` binary:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

---

## 1. Writing `demo_debug.ks` and Emitting `demo_debug.o`

Create a five-line Kaleidoscope source file named `demo_debug.ks` that computes `n!` (`factorial`):

```text
def factorial(n: int) -> int
  var acc: int = 1 in
  (for i: int = 1, i < n in
    acc = acc * i) :
  acc;
```

:::{note}
**Remember Kaleidoscope's Post-Tested `for` Loop**:
In Kaleidoscope, `for i: int = 1, i < n in acc = acc * i` executes the loop body *before* evaluating the continuation condition `i < n` and incrementing `i`.  When `n = 5`, the loop runs for `i = 1, 2, 3, 4`, and `5` (stopping when `5 < 5` is false), computing `1 * 1 * 2 * 3 * 4 * 5 = 120`!
:::

Compile `demo_debug.ks` into a native object file with DWARF debug info enabled:

```bash
./kaleidoscope -g demo_debug.ks --emit-obj demo_debug.o
```

---

## 2. Inspecting DWARF DIEs with `llvm-dwarfdump`

Before launching a debugger, let us look directly inside `demo_debug.o` using LLVM's **`llvm-dwarfdump`** utility:

```bash
llvm-dwarfdump demo_debug.o
```

You will see the complete DWARF v4 tree emitted by your `DIBuilder` code:

```text
demo_debug.o:	file format elf64-x86-64

.debug_info contents:
0x00000000: Compile Unit: length = 0x00000072, format = DWARF32, version = 0x0004, abbr_offset = 0x0000, addr_size = 0x08 (next unit at 0x00000076)

0x0000000b: DW_TAG_compile_unit
              DW_AT_producer	("Kaleidoscope Compiler")
              DW_AT_language	(DW_LANG_C)
              DW_AT_name	("demo_debug.ks")
              DW_AT_stmt_list	(0x00000000)
              DW_AT_comp_dir	(".")
              DW_AT_GNU_pubnames	(true)
              DW_AT_low_pc	(0x0000000000000000)
              DW_AT_high_pc	(0x0000000000000041)

0x0000002a:   DW_TAG_subprogram
                DW_AT_low_pc	(0x0000000000000000)
                DW_AT_high_pc	(0x0000000000000041)
                DW_AT_frame_base	(DW_OP_reg7 RSP)
                DW_AT_name	("factorial")
                DW_AT_decl_file	("./demo_debug.ks")
                DW_AT_decl_line	(1)
                DW_AT_prototyped	(true)
                DW_AT_type	(0x0000006e "int")
                DW_AT_external	(true)

0x00000043:     DW_TAG_formal_parameter
                  DW_AT_location	(DW_OP_fbreg -4)
                  DW_AT_name	("n")
                  DW_AT_decl_file	("./demo_debug.ks")
                  DW_AT_decl_line	(1)
                  DW_AT_type	(0x0000006e "int")

0x00000051:     DW_TAG_variable
                  DW_AT_location	(DW_OP_fbreg -12)
                  DW_AT_name	("acc")
                  DW_AT_decl_file	("./demo_debug.ks")
                  DW_AT_decl_line	(5)
                  DW_AT_type	(0x0000006e "int")

0x0000005f:     DW_TAG_variable
                  DW_AT_location	(DW_OP_fbreg -8)
                  DW_AT_name	("i")
                  DW_AT_decl_file	("./demo_debug.ks")
                  DW_AT_decl_line	(4)
                  DW_AT_type	(0x0000006e "int")

0x0000006d:     NULL

0x0000006e:   DW_TAG_base_type
                DW_AT_name	("int")
                DW_AT_encoding	(DW_ATE_signed)
                DW_AT_byte_size	(0x04)

0x00000075:   NULL
```

Look at those `DW_AT_location` attributes under `DW_TAG_subprogram ("factorial")`: LLVM's x86-64 backend lowered our three `#dbg_declare` `alloca` slots into exact stack-frame offsets relative to `DW_AT_frame_base (RSP)`:

```text
  Stack Frame of factorial(n: int) on x86-64 (Red Zone below RSP)
  +------------------+------------------------+----------------------------+
  | RSP - 12         | RSP - 8                | RSP - 4                    |
  | acc: int (4B)    | i: int (4B)            | n: int (4B)                |
  | DW_OP_fbreg -12  | DW_OP_fbreg -8         | DW_OP_fbreg -4             |
  +------------------+------------------------+----------------------------+
```

---

## 3. Linking `debug_app` and Stepping Live in GDB

Create a tiny C entry point `debug_main.c` (also provided in [code/debug_main.c](../code/debug_main.c)) that calls `factorial(5)`:

```c
#include <stdint.h>
#include <stdio.h>

int32_t factorial(int32_t n);

int main(void) {
  int32_t result = factorial(5);
  printf("factorial(5) = %d\n", result);
  return 0;
}
```

Link `debug_main.c` and `demo_debug.o` into a native executable:

```bash
clang -g debug_main.c demo_debug.o -o debug_app
```

Now launch `gdb ./debug_app`, set a breakpoint on `demo_debug.ks:4`, and watch `i` and `acc` evolve across iterations:

```text
(gdb) break demo_debug.ks:4
Breakpoint 1 at 0x118c: file demo_debug.ks, line 4.
(gdb) run
Starting program: ./debug_app

Breakpoint 1, factorial (n=5) at demo_debug.ks:4
4	    acc = acc * i) :
(gdb) info args
n = 5
(gdb) next
3	  (for i: int = 1, i < n in
(gdb) print i
$1 = 1
(gdb) print acc
$2 = 1
(gdb) next
4	    acc = acc * i) :
(gdb) next
3	  (for i: int = 1, i < n in
(gdb) print i
$3 = 2
(gdb) print acc
$4 = 2
(gdb) next
4	    acc = acc * i) :
(gdb) next
3	  (for i: int = 1, i < n in
(gdb) print i
$5 = 3
(gdb) print acc
$6 = 6
(gdb) continue
Continuing.
factorial(5) = 120
[Inferior 1 (process 1025142) exited normally]
```

Pause for a second and appreciate what just happened: **`gdb` is reading your `.ks` source file on disk, stopping on Kaleidoscope line numbers, decoding `n = 5` in `info args`, and watching `acc` step through `1 -> 2 -> 6 -> 120`!**

---

## GDB vs LLDB Quick Command Reference

Prefer **`lldb`** (the default LLVM debugger on macOS and available across Linux distributions)?  Your `demo_debug.o` binary works identically in `lldb ./debug_app` using the equivalent commands below:

| Debugging Action | GDB Command (`gdb ./debug_app`) | LLDB Command (`lldb ./debug_app`) |
| :--- | :--- | :--- |
| Break at `file:line` | `break demo_debug.ks:4` | `breakpoint set -f demo_debug.ks -l 4` (or `b demo_debug.ks:4`) |
| Break on function name | `break factorial` | `breakpoint set -n factorial` (or `b factorial`) |
| Start execution | `run` | `run` (or `r`) |
| Inspect function arguments | `info args` | `frame variable -a` (or `fr v -a`) |
| Inspect all local variables | `info locals` | `frame variable` (or `fr v`) |
| Print expression / variable | `print acc` (or `p acc`) | `expr acc` (or `p acc`) |
| Step over next source line | `next` (or `n`) | `next` (or `n`) |
| Step into function call | `step` (or `s`) | `step` (or `s`) |
| Resume until next breakpoint | `continue` (or `c`) | `continue` (or `c`) |

In [Section 16](../16-custom-opt-pass/index.md), our final capstone section, you will step across the boundary from frontend compiler author to **middle-end LLVM pass engineer**, writing and registering your own custom IR optimisation pass in LLVM's modern New Pass Manager!

---

[Previous: Subprograms, Variables, and Source Locations](02-subprograms-locations.md) | [Section Overview](index.md) | [Next: The Modern LLVM Pass Pipeline](../16-custom-opt-pass/01-pass-pipeline.md)
