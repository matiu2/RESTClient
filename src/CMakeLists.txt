project (src)

add_library(RESTClient HTTP.cpp)
target_link_libraries(RESTClient ${CPP} ${CMAKE_THREAD_LIBS_INIT} ${Boost_SYSTEM_LIBRARY} ${Boost_COROUTINE_LIBRARY} ${OPENSSL_LIBRARIES})

if (${BUILD_TESTS})
  add_executable(testHTTP testHTTP.cpp)
  target_link_libraries(testHTTP RESTClient ${Boost_REGEX_LIBRARY})
  add_test(testHTTP testHTTP)
endif(${BUILD_TESTS})
