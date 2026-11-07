set(LLVM_LINK_COMPONENTS
  AllTargetsAsmParsers
  AllTargetsCodeGens
  AllTargetsDescs
  AllTargetsInfos
  Analysis
  Core
  ExecutionEngine
  InstCombine
  MC
  Object
  OrcJIT
  Passes
  RuntimeDyld
  ScalarOpts
  Support
  Target
  TargetParser
  TransformUtils
  native
  )

add_kaleidoscope_chapter(Kaleidoscope-Sec10
  kaleidoscope.cpp
  )

target_include_directories(Kaleidoscope-Sec10 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  )
