cmake_minimum_required(VERSION 2.8)

project (RESTClient)

option(IS_CLANG "Are we using the clang compiler?" ON)

if(${IS_CLANG})
    add_definitions(-fPIC -std=c++1z -stdlib=libc++)
endif()

# Clang/LLVM libc++
find_library(CPP c++)
set(CPP_V1_INCLUDE_DIR /usr/include/c++/v1 CACHE PATH "The location of LLVM's STL")
include_directories(${CPP_V1_INCLUDE_DIR})

# Nice default debug settings so that we can view the contents of strings in gdb
if(${IS_CLANG})
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -fstandalone-debug -fwritable-strings -fno-builtin"
    CACHE STRING "Flags used by the compiler during debug builds." FORCE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
endif()

# options

option(BUILD_TESTS "Build the test suite" OFF)
option(HTTP_ON_STD_OUT "Copy HTTP sent and received to stdout" OFF)
set(MIN_LOG_LEVEL ERROR NONE CACHE STRING "How much to log to std::clog. NONE, TRACE, DEBUG, INFO, WARN, ERROR, or FATAL")
option(LOG_LOCATION "Log the location in the files of log messages" OFF)
if (${BUILD_TESTS})
    option(BUILD_RS_TESTS "Build tests that require a Rackspace API login?" OFF)
endif()

if (${BUILD_TESTS} AND ${BUILD_RS_TESTS})
    SET(RS_USERNAME "" CACHE STRING "Rackspace API username, for RS tests")
    SET(RS_APIKEY "" CACHE STRING "Rackspace API apikey")
    SET(RS_REGION "" CACHE STRING "Rackspace datacerter SYD, DFW, ORD, LON, or IAD (at time of writing)")
    SET(RS_CONTAINER_NAME "" CACHE STRING "Rackspace container name, CONTENTS MAY BE OVERWRITTEN")
endif()

# extra definitions
add_definitions(-DMIN_LOG_LEVEL=${MIN_LOG_LEVEL})

if (${LOG_LOCATION})
add_definitions(-DLOG_LOCATION=1)
endif()

if (${HTTP_ON_STD_OUT})
  add_definitions(-DHTTP_ON_STD_OUT)
endif()

if (${BUILD_RS_TESTS})
  add_definitions(-DBUILD_RS_TESTS)
  add_definitions(-DRS_USERNAME="${RS_USERNAME}")
  add_definitions(-DRS_APIKEY="${RS_APIKEY}")
  add_definitions(-DRS_REGION="${RS_REGION}")
  add_definitions(-DRS_CONTAINER_NAME="${RS_CONTAINER_NAME}")
endif()

# Dependencies

include(cmake/dependencies.cmake)

# Tests

if (${BUILD_TESTS})
  enable_testing()
endif(${BUILD_TESTS})

# Sub directories

add_subdirectory(src)
add_subdirectory(experiments)
add_subdirectory(examples)
