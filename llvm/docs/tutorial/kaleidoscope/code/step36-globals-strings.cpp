#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdio>
#include <memory>

int main() {
  auto Context = std::make_unique<llvm::LLVMContext>();
  auto Module = std::make_unique<llvm::Module>("globals_demo", *Context);
  auto Builder = std::make_unique<llvm::IRBuilder<>>(*Context);

  llvm::Type *I32Ty = llvm::Type::getInt32Ty(*Context);
  llvm::Type *PtrTy = llvm::PointerType::get(*Context, 0);

  // 1. Mutable global variable: int my_global = 42;
  auto *GlobalVar = new llvm::GlobalVariable(
      *Module, I32Ty, false, llvm::GlobalValue::ExternalLinkage,
      llvm::ConstantInt::get(I32Ty, 42), "my_global");

  // 2. Function calling puts with a global string literal:
  llvm::FunctionType *FT = llvm::FunctionType::get(I32Ty, {}, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, "print_message", Module.get());

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*Context, "entry", F);
  Builder->SetInsertPoint(BB);

  // Create global string in .rodata
  llvm::Value *StrPtr =
      Builder->CreateGlobalString("Hello, Systems LLVM!\n", "str");

  // Declare extern int puts(const char *);
  llvm::FunctionType *PutsFT = llvm::FunctionType::get(I32Ty, {PtrTy}, false);
  llvm::FunctionCallee PutsFn = Module->getOrInsertFunction("puts", PutsFT);

  Builder->CreateCall(PutsFn, StrPtr);

  // Load from my_global, increment, store back
  llvm::Value *Loaded = Builder->CreateLoad(I32Ty, GlobalVar, "loaded");
  llvm::Value *Inc = Builder->CreateAdd(Loaded, Builder->getInt32(1), "inc");
  Builder->CreateStore(Inc, GlobalVar);

  Builder->CreateRet(Inc);

  llvm::verifyFunction(*F);
  Module->print(llvm::errs(), nullptr);
  return 0;
}
