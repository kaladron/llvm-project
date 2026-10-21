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
#include "llvm/Support/raw_ostream.h"

#include <memory>

class ZeroIdentityPass : public llvm::PassInfoMixin<ZeroIdentityPass> {
public:
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &) {
    bool Modified = false;
    for (auto &BB : F) {
      for (auto Inst = BB.begin(), E = BB.end(); Inst != E;) {
        llvm::Instruction &I = *Inst++;
        if (I.getOpcode() == llvm::Instruction::Add) {
          if (auto *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(1))) {
            if (CI->isZero()) {
              I.replaceAllUsesWith(I.getOperand(0));
              I.eraseFromParent();
              Modified = true;
            }
          }
        }
      }
    }
    return Modified ? llvm::PreservedAnalyses::none()
                    : llvm::PreservedAnalyses::all();
  }
};

int main() {
  auto Context = std::make_unique<llvm::LLVMContext>();
  auto Module = std::make_unique<llvm::Module>("pass_demo", *Context);
  auto Builder = std::make_unique<llvm::IRBuilder<>>(*Context);

  llvm::Type *I32Ty = llvm::Type::getInt32Ty(*Context);
  llvm::FunctionType *FT = llvm::FunctionType::get(I32Ty, {I32Ty}, false);
  llvm::Function *F = llvm::Function::Create(
      FT, llvm::Function::ExternalLinkage, "identity_test", Module.get());

  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*Context, "entry", F);
  Builder->SetInsertPoint(BB);

  llvm::Value *Arg = F->arg_begin();
  llvm::Value *AddZero =
      Builder->CreateAdd(Arg, Builder->getInt32(0), "addzero");
  Builder->CreateRet(AddZero);

  fprintf(stderr, "=== Before Pass ===\n");
  Module->print(llvm::errs(), nullptr);

  // Run custom pass
  llvm::FunctionAnalysisManager FAM;
  llvm::PassBuilder PB;
  PB.registerFunctionAnalyses(FAM);

  llvm::FunctionPassManager FPM;
  FPM.addPass(ZeroIdentityPass());
  FPM.run(*F, FAM);

  fprintf(stderr, "\n=== After Pass (x + 0 eliminated) ===\n");
  Module->print(llvm::errs(), nullptr);
  return 0;
}
