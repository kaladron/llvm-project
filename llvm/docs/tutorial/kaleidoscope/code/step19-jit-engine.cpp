#include "KaleidoscopeJIT.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdio>
#include <memory>

static std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
static llvm::ExitOnError ExitOnErr;

int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());

  printf("Kaleidoscope JIT Initialisation (Step 5.2)\n");
  printf("JIT execution session created successfully.\n");
  printf("Default data layout: %s\n",
         TheJIT->getDataLayout().getStringRepresentation().c_str());

  return 0;
}
