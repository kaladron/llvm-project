add_kaleidoscope_chapter(Kaleidoscope-Sec03
  kaleidoscope.cpp
  )

if(MSVC)
  target_link_libraries(Kaleidoscope-Sec03 PRIVATE "-ignore:4199")
endif()
