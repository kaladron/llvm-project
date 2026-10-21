#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <memory>

int main() {
  auto Context = std::make_unique<llvm::LLVMContext>();
  auto Module = std::make_unique<llvm::Module>("debug_demo", *Context);
  auto Builder = std::make_unique<llvm::IRBuilder<>>(*Context);

  llvm::DIBuilder DIB(*Module);

  // 1. DIFile and DICompileUnit
  llvm::DIFile *File = DIB.createFile("demo.c", "/src");
  llvm::DICompileUnit *CU = DIB.createCompileUnit(
      llvm::dwarf::DW_LANG_C99, File, "Kaleidoscope201", false, "", 0);

  // 2. Function prototype
  llvm::Type *I32Ty = llvm::Type::getInt32Ty(*Context);
  llvm::FunctionType *FT = llvm::FunctionType::get(I32Ty, {I32Ty}, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, "add_one", Module.get());

  // 3. DISubprogram
  llvm::DISubroutineType *DIType =
      DIB.createSubroutineType(DIB.getOrCreateTypeArray(
          {DIB.createBasicType("int", 32, llvm::dwarf::DW_ATE_signed)}));
  llvm::DISubprogram *SP = DIB.createFunction(
      File, "add_one", "add_one", File, 5, DIType, 5,
      llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
  F->setSubprogram(SP);

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*Context, "entry", F);
  Builder->SetInsertPoint(BB);

  // Set line number 6 on instruction
  Builder->SetCurrentDebugLocation(llvm::DILocation::get(*Context, 6, 3, SP));
  llvm::Value *Arg = F->arg_begin();
  llvm::Value *Add = Builder->CreateAdd(Arg, Builder->getInt32(1), "add");

  // Set line number 7 on return
  Builder->SetCurrentDebugLocation(llvm::DILocation::get(*Context, 7, 3, SP));
  Builder->CreateRet(Add);

  // Finalise DWARF metadata
  DIB.finalize();

  llvm::verifyFunction(*F);
  Module->print(llvm::errs(), nullptr);
  return 0;
}
