set(LLVM_LINK_COMPONENTS
  Analysis
  Core
  ExecutionEngine
  Object
  OrcJIT
  RuntimeDyld
  Support
  TargetParser
  native
  )

add_kaleidoscope_chapter(Kaleidoscope-Sec05
  kaleidoscope.cpp
  )

target_include_directories(Kaleidoscope-Sec05 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  )
