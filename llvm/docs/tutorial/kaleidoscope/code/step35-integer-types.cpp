#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
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

enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
  tok_int_literal = -6,
  tok_type_int = -7,
  tok_type_double = -8,
  tok_type_bool = -9,
};

static std::string IdentifierStr;
static double NumVal;
static int64_t IntVal;

static int gettok() {
  static int LastChar = ' ';
  while (isspace(LastChar))
    LastChar = getchar();

  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "int")
      return tok_type_int;
    if (IdentifierStr == "double")
      return tok_type_double;
    if (IdentifierStr == "bool")
      return tok_type_bool;
    return tok_identifier;
  }

  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    bool HasDot = false;
    do {
      if (LastChar == '.')
        HasDot = true;
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    if (HasDot) {
      NumVal = strtod(NumStr.c_str(), nullptr);
      return tok_number;
    }
    IntVal = strtoll(NumStr.c_str(), nullptr, 10);
    return tok_int_literal;
  }

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

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual llvm::Value *codegen() = 0;
  virtual llvm::Type *getType() = 0;
};

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;
static std::map<std::string, llvm::Value *> NamedValues;

class IntExprAST : public ExprAST {
  int64_t Val;

public:
  IntExprAST(int64_t Val) : Val(Val) {}
  llvm::Value *codegen() override {
    return Builder->getInt32(static_cast<int32_t>(Val));
  }
  llvm::Type *getType() override { return Builder->getInt32Ty(); }
};

class DoubleExprAST : public ExprAST {
  double Val;

public:
  DoubleExprAST(double Val) : Val(Val) {}
  llvm::Value *codegen() override {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val));
  }
  llvm::Type *getType() override { return Builder->getDoubleTy(); }
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  llvm::Value *codegen() override {
    llvm::Value *V = NamedValues[Name];
    if (!V) {
      fprintf(stderr, "Error: Unknown variable name %s\n", Name.c_str());
      return nullptr;
    }
    return V;
  }
  llvm::Type *getType() override {
    llvm::Value *V = NamedValues[Name];
    return V ? V->getType() : nullptr;
  }
};

class BinaryExprAST : public ExprAST {
  std::string Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(std::string Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(std::move(Op)), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  llvm::Value *codegen() override {
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (!L || !R)
      return nullptr;

    bool IsInt = L->getType()->isIntegerTy();

    if (Op == "+")
      return IsInt ? Builder->CreateAdd(L, R, "addtmp")
                   : Builder->CreateFAdd(L, R, "faddtmp");
    if (Op == "-")
      return IsInt ? Builder->CreateSub(L, R, "subtmp")
                   : Builder->CreateFSub(L, R, "fsubtmp");
    if (Op == "*")
      return IsInt ? Builder->CreateMul(L, R, "multmp")
                   : Builder->CreateFMul(L, R, "fmultmp");
    if (Op == "/s")
      return Builder->CreateSDiv(L, R, "sdivtmp");
    if (Op == "/u")
      return Builder->CreateUDiv(L, R, "udivtmp");
    if (Op == "/")
      return IsInt ? Builder->CreateSDiv(L, R, "sdivtmp")
                   : Builder->CreateFDiv(L, R, "fdivtmp");
    if (Op == "%s")
      return Builder->CreateSRem(L, R, "sremtmp");
    if (Op == "%u")
      return Builder->CreateURem(L, R, "uremtmp");
    if (Op == "&")
      return Builder->CreateAnd(L, R, "andtmp");
    if (Op == "|")
      return Builder->CreateOr(L, R, "ortmp");
    if (Op == "^")
      return Builder->CreateXor(L, R, "xortmp");
    if (Op == "<<")
      return Builder->CreateShl(L, R, "shltmp");
    if (Op == ">>s")
      return Builder->CreateAShr(L, R, "ashrtmp");
    if (Op == ">>u")
      return Builder->CreateLShr(L, R, "lshrtmp");

    // Comparisons
    if (Op == "<s")
      return Builder->CreateICmpSLT(L, R, "cmpslt");
    if (Op == "<u")
      return Builder->CreateICmpULT(L, R, "cmpult");
    if (Op == "==")
      return IsInt ? Builder->CreateICmpEQ(L, R, "icmpeq")
                   : Builder->CreateFCmpOEQ(L, R, "fcmpeq");

    fprintf(stderr, "Error: Unknown binary operator %s\n", Op.c_str());
    return nullptr;
  }

  llvm::Type *getType() override {
    if (Op == "<s" || Op == "<u" || Op == "==")
      return Builder->getInt1Ty();
    return LHS->getType();
  }
};

class CastExprAST : public ExprAST {
  std::string TargetType;
  std::unique_ptr<ExprAST> Operand;

public:
  CastExprAST(std::string TargetType, std::unique_ptr<ExprAST> Operand)
      : TargetType(std::move(TargetType)), Operand(std::move(Operand)) {}

  llvm::Value *codegen() override {
    llvm::Value *V = Operand->codegen();
    if (!V)
      return nullptr;

    if (TargetType == "double")
      return Builder->CreateSIToFP(V, Builder->getDoubleTy(), "todouble");
    if (TargetType == "int")
      return Builder->CreateFPToSI(V, Builder->getInt32Ty(), "toint");
    if (TargetType == "bool")
      return Builder->CreateICmpNE(V, Builder->getInt32(0), "tobool");

    fprintf(stderr, "Error: Unknown cast target type %s\n", TargetType.c_str());
    return nullptr;
  }

  llvm::Type *getType() override {
    if (TargetType == "double")
      return Builder->getDoubleTy();
    if (TargetType == "int")
      return Builder->getInt32Ty();
    return Builder->getInt1Ty();
  }
};

int main() {
  TheContext = std::make_unique<llvm::LLVMContext>();
  TheModule = std::make_unique<llvm::Module>("kaleidoscope_types", *TheContext);
  Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);

  // Define: int compute(int a, int b)
  llvm::Type *I32 = Builder->getInt32Ty();
  llvm::FunctionType *FT = llvm::FunctionType::get(I32, {I32, I32}, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, "compute", TheModule.get());

  auto Args = F->arg_begin();
  llvm::Value *ArgA = Args++;
  ArgA->setName("a");
  llvm::Value *ArgB = Args++;
  ArgB->setName("b");

  NamedValues["a"] = ArgA;
  NamedValues["b"] = ArgB;

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", F);
  Builder->SetInsertPoint(BB);

  // AST: (a + b) * (a /s b)
  auto LHS = std::make_unique<BinaryExprAST>(
      "+", std::make_unique<VariableExprAST>("a"),
      std::make_unique<VariableExprAST>("b"));
  auto RHS = std::make_unique<BinaryExprAST>(
      "/s", std::make_unique<VariableExprAST>("a"),
      std::make_unique<VariableExprAST>("b"));
  auto Expr =
      std::make_unique<BinaryExprAST>("*", std::move(LHS), std::move(RHS));

  llvm::Value *RetVal = Expr->codegen();
  Builder->CreateRet(RetVal);

  llvm::verifyFunction(*F);
  TheModule->print(llvm::errs(), nullptr);
  return 0;
}
