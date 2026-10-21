#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
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
  auto Module = std::make_unique<llvm::Module>("struct_gep_demo", *Context);
  auto Builder = std::make_unique<llvm::IRBuilder<>>(*Context);

  llvm::Type *I32Ty = llvm::Type::getInt32Ty(*Context);
  llvm::Type *DoubleTy = llvm::Type::getDoubleTy(*Context);
  llvm::Type *PtrTy = llvm::PointerType::get(*Context, 0);

  // Define struct Point { int32_t x; double y; };
  llvm::StructType *PointTy =
      llvm::StructType::create(*Context, {I32Ty, DoubleTy}, "Point");

  // Inspect DataLayout sizing and alignment
  const llvm::DataLayout &DL = Module->getDataLayout();
  uint64_t StructSize = DL.getTypeAllocSize(PointTy);
  llvm::Align StructAlign = DL.getABITypeAlign(PointTy);
  fprintf(stderr, "Struct Point size: %lu bytes, align: %u bytes\n", StructSize,
          (unsigned)StructAlign.value());

  // Function: double get_point_y(Point *p)
  llvm::FunctionType *FT = llvm::FunctionType::get(DoubleTy, {PtrTy}, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, "get_point_y", Module.get());

  llvm::Value *PtrArg = F->arg_begin();
  PtrArg->setName("p");

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*Context, "entry", F);
  Builder->SetInsertPoint(BB);

  // Compute GEP to field 1 (y): gep Point, ptr %p, i32 0, i32 1
  llvm::Value *YPtr = Builder->CreateStructGEP(PointTy, PtrArg, 1, "y_ptr");
  llvm::Value *YVal = Builder->CreateLoad(DoubleTy, YPtr, "y_val");

  Builder->CreateRet(YVal);

  llvm::verifyFunction(*F);
  Module->print(llvm::errs(), nullptr);
  return 0;
}
