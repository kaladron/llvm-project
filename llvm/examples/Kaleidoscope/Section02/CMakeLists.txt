add_kaleidoscope_chapter(Kaleidoscope-Sec02
  kaleidoscope.cpp
  )

if(MSVC)
  target_link_libraries(Kaleidoscope-Sec02 PRIVATE "-ignore:4199")
endif()
