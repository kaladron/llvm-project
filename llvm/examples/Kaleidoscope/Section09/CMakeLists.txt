set(LLVM_LINK_COMPONENTS
  Analysis
  Core
  ExecutionEngine
  InstCombine
  Object
  OrcJIT
  Passes
  RuntimeDyld
  ScalarOpts
  Support
  TargetParser
  TransformUtils
  native
  )

add_kaleidoscope_chapter(Kaleidoscope-Sec09
  kaleidoscope.cpp
  )

target_include_directories(Kaleidoscope-Sec09 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  )
