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
#include "llvm/Support/raw_ostream.h"

#include <cstdio>
#include <memory>

int main() {
  auto Context = std::make_unique<llvm::LLVMContext>();
  auto Module = std::make_unique<llvm::Module>("pointers_demo", *Context);
  auto Builder = std::make_unique<llvm::IRBuilder<>>(*Context);

  llvm::Type *I32Ty = Builder->getInt32Ty();
  llvm::Type *I64Ty = Builder->getInt64Ty();
  llvm::Type *PtrTy = Builder->getPtrTy();

  // Function: int32_t heap_demo(int32_t initial_val)
  llvm::FunctionType *FT = llvm::FunctionType::get(I32Ty, {I32Ty}, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, "heap_demo", Module.get());

  llvm::Value *InitVal = F->arg_begin();
  InitVal->setName("initial_val");

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*Context, "entry", F);
  Builder->SetInsertPoint(BB);

  // Declare extern void *malloc(size_t)
  llvm::FunctionType *MallocFT = llvm::FunctionType::get(PtrTy, {I64Ty}, false);
  llvm::FunctionCallee MallocFn =
      Module->getOrInsertFunction("malloc", MallocFT);

  // Declare extern void free(void *)
  llvm::FunctionType *FreeFT =
      llvm::FunctionType::get(Builder->getVoidTy(), {PtrTy}, false);
  llvm::FunctionCallee FreeFn = Module->getOrInsertFunction("free", FreeFT);

  // 1. Allocate 4 bytes on heap: int *p = (int*)malloc(sizeof(int));
  llvm::Value *Ptr =
      Builder->CreateCall(MallocFn, Builder->getInt64(4), "heap_ptr");

  // 2. Dereference write: *p = initial_val;
  Builder->CreateStore(InitVal, Ptr);

  // 3. Dereference read: int result = *p + 10;
  llvm::Value *Loaded = Builder->CreateLoad(I32Ty, Ptr, "loaded_val");
  llvm::Value *Result =
      Builder->CreateAdd(Loaded, Builder->getInt32(10), "result");

  // 4. Free heap memory: free(p);
  Builder->CreateCall(FreeFn, Ptr);

  // 5. Return result
  Builder->CreateRet(Result);

  llvm::verifyFunction(*F);
  Module->print(llvm::errs(), nullptr);
  return 0;
}
