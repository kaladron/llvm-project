#include "llvm/Support/TargetSelect.h"
#include <cstdio>

int main() {
  // Initialize native target machine, asm printer, and asm parser.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  printf("Kaleidoscope Target Initialisation (Step 5.1)\n");
  printf("Native host target machine initialized successfully.\n");

  return 0;
}
