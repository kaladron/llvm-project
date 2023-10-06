if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${LIBC_TARGET_OS})
endif()

add_object_library(
  time_utils
  SRCS
    time_utils.cpp
  HDRS
    time_utils.h
  DEPENDS
    libc.include.time
    libc.src.errno.errno
    .time_zone_posix
)

add_object_library(
  time_zone_posix
  CXX_STANDARD 20
  SRCS
    time_zone_posix.cpp
  HDRS
    time_zone_posix.h
  DEPENDS
    libc.src.ctype.isdigit
    libc.src.stdlib.strtol
    libc.src.string.memory_utils.memset_implementation
    libc.src.string.strchr
    libc.src.__support.CPP.optional
    libc.src.__support.str_to_integer
)

add_entrypoint_object(
  asctime
  SRCS
    asctime.cpp
  HDRS
    asctime.h
  DEPENDS
    .time_utils
    libc.include.time
)

add_entrypoint_object(
  asctime_r
  SRCS
    asctime_r.cpp
  HDRS
    asctime_r.h
  DEPENDS
    .time_utils
    libc.include.time
)

add_entrypoint_object(
  difftime
  SRCS
    difftime.cpp
  HDRS
    difftime.h
  DEPENDS
    libc.include.time
)

add_entrypoint_object(
  gmtime
  SRCS
    gmtime.cpp
  HDRS
    gmtime.h
  DEPENDS
    .time_utils
    libc.include.time
)

add_entrypoint_object(
  gmtime_r
  SRCS
    gmtime_r.cpp
  HDRS
    gmtime_r.h
  DEPENDS
    .time_utils
    libc.include.time
)

add_entrypoint_object(
  mktime
  SRCS
    mktime.cpp
  HDRS
    mktime.h
  DEPENDS
    .time_utils
    libc.include.time
    libc.src.errno.errno
)

add_entrypoint_object(
  time
  ALIAS
  DEPENDS
    .${LIBC_TARGET_OS}.time
)

add_entrypoint_object(
  clock
  ALIAS
  DEPENDS
    .${LIBC_TARGET_OS}.clock
)

add_entrypoint_object(
  nanosleep
  ALIAS
  DEPENDS
    .${LIBC_TARGET_OS}.nanosleep
)

add_entrypoint_object(
  clock_gettime
  ALIAS
  DEPENDS
    .${LIBC_TARGET_OS}.clock_gettime
)

add_entrypoint_object(
  gettimeofday
  ALIAS
  DEPENDS
    .${LIBC_TARGET_OS}.gettimeofday
)
