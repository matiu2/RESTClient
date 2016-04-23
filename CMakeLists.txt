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

option(BUILD_TESTS "Build the test suite" OFF)
option(HTTP_ON_STD_OUT "Copy HTTP sent and received to stdout" OFF)

FIND_PACKAGE(Boost 1.60 REQUIRED COMPONENTS system coroutine)
FIND_PACKAGE(OpenSSL REQUIRED)

include_directories(${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES})

if (${BUILD_TESTS})
  enable_testing()
endif(${BUILD_TESTS})

if (${HTTP_ON_STD_OUT})
  add_definitions(-DHTTP_ON_STD_OUT)
endif()

add_subdirectory(src)
add_subdirectory(experiments)
