add_object_library(
  h_errno
  SRCS
    h_errno.cpp
  HDRS
    h_errno.h
  DEPENDS
    libc.src.__support.macros.attributes
    libc.src.__support.macros.config
)

if(NOT TARGET libc.src.__support.File.file OR
    NOT TARGET libc.src.__support.File.platform_file)
  return()
endif()

add_header_library(
  line_reader
  HDRS
    line_reader.h
  DEPENDS
    libc.hdr.types.size_t
    libc.src.__support.CPP.optional
    libc.src.__support.CPP.string_view
    libc.src.__support.CPP.stringstream
    libc.src.__support.File.file
    libc.src.__support.File.platform_file
    libc.src.__support.ctype_utils
    libc.src.__support.error_or
    libc.src.__support.macros.config
)
