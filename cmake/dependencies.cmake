
# Threads
find_package(Threads)

# Boost
FIND_PACKAGE(Boost 1.60 REQUIRED COMPONENTS system coroutine iostreams)
include_directories(${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES})

if (${BUILD_TESTS})
  FIND_PACKAGE(Boost 1.60 REQUIRED COMPONENTS regex)
endif(${BUILD_TESTS})

# OpenSSL
FIND_PACKAGE(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR} ${OPENSSL_LIBRARIES})

# json_spirit - JSON wrapper
if (${BUILD_TESTS} AND ${BUILD_RS_TESTS})

  include(ExternalProject)

  ExternalProject_Add(json_spirit
      PREFIX 3rd_party
      GIT_REPOSITORY https://github.com/cierelabs/json_spirit
      CMAKE_CACHE_ARGS "-DCMAKE_CXX_FLAGS:string=-fPIC -std=c++1z -stdlib=libc++"
      TLS_VERIFY true
      TLS_CAINFO certs/DigiCertHighAssuranceEVRootCA.crt
      TEST_BEFORE_INSTALL 0
      TEST_COMMAND ctest
      UPDATE_COMMAND "" # Skip annoying update attempts for every build
      INSTALL_COMMAND ""
  )
  SET(JSON_SPIRIT_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/3rd_party/src/json_spirit/ciere)
  include_directories(${JSON_SPIRIT_SOURCE_DIR})
  find_library(JSON_LIB json
               PATHS ${CMAKE_CURRENT_BINARY_DIR}/3rd_party/src/json_spirit-build
               NO_DEFAULT_PATH)
endif()
