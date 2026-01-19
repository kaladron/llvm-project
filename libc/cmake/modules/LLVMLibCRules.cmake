include(LLVMLibCCompileOptionRules)
include(LLVMLibCTargetNameUtils)
include(LLVMLibCFlagRules)
include(LLVMLibCObjectRules)
include(LLVMLibCLibraryRules)
include(LLVMLibCTestRules)

# Include lit test infrastructure if enabled
if(LIBC_ENABLE_LIT_TESTS)
  include(LLVMLibCLitTestRules)
endif()
