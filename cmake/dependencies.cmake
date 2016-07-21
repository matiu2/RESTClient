
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

# jsonpp11 - JSON wrapper
if (${BUILD_TESTS} AND ${BUILD_RS_TESTS})

  include(ExternalProject)

  ExternalProject_Add(jsonpp11
      PREFIX 3rd_party
      GIT_REPOSITORY https://github.com/matiu2/jsonpp11.git
      TLS_VERIFY true
      TLS_CAINFO certs/DigiCertHighAssuranceEVRootCA.crt
      TEST_BEFORE_INSTALL 0
      TEST_COMMAND ctest
      UPDATE_COMMAND "" # Skip annoying update attempts for every build
      INSTALL_COMMAND ""
  )
  SET(JSONPP11_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/3rd_party/src/jsonpp11/src)
  include_directories(${JSONPP11_SOURCE_DIR})
endif()
