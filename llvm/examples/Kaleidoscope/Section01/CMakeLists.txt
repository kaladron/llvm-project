add_kaleidoscope_chapter(Kaleidoscope-Sec01
  kaleidoscope.cpp
  )

if(MSVC)
  target_link_libraries(Kaleidoscope-Sec01 PRIVATE "-ignore:4199")
endif()
