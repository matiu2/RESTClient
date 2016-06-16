cmake_minimum_required(VERSION 2.8)

project (RESTClient)

add_definitions(-fPIC -std=c++1z -stdlib=libc++)

# Clang/LLVM libc++
find_library(CPP c++)
set(CPP_V1_INCLUDE_DIR /usr/include/c++/v1 CACHE PATH "The location of LLVM's STL")
include_directories(${CPP_V1_INCLUDE_DIR})

# Threads
find_package(Threads)

# Nice default debug settings so that we can view the contents of strings in gdb
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fstandalone-debug -fwritable-strings -fno-builtin"
    CACHE STRING "Flags used by the compiler during debug builds." FORCE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

# Dependencies

FIND_PACKAGE(Boost 1.60 REQUIRED COMPONENTS system coroutine iostreams)
FIND_PACKAGE(OpenSSL REQUIRED)

include_directories(${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES})

# options

option(BUILD_TESTS "Build the test suite" OFF)
option(HTTP_ON_STD_OUT "Copy HTTP sent and received to stdout" OFF)
set(MIN_LOG_LEVEL ERROR CACHE STRING "How much to log to std::clog. NONE, TRACE, DEBUG, INFO, WARN, ERROR, or FATAL")
option(LOG_LOCATION "Log the location in the files of log messages" OFF)

# extra definitions
add_definitions(-DMIN_LOG_LEVEL=${MIN_LOG_LEVEL})
if (${LOG_LOCATION})
add_definitions(-DLOG_LOCATION=1)
endif()
if (${HTTP_ON_STD_OUT})
  add_definitions(-DHTTP_ON_STD_OUT)
endif()

# Tests

if (${BUILD_TESTS})
  FIND_PACKAGE(Boost 1.60 REQUIRED COMPONENTS regex)
  enable_testing()
endif(${BUILD_TESTS})

# Sub directories

add_subdirectory(src)
add_subdirectory(experiments)
