project (src)

add_library(RESTClient HTTP.cpp)
target_link_libraries(RESTClient ${CPP} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})

if (${BUILD_TESTS})
  add_executable(testHTTP testHTTP.cpp)
  target_link_libraries(testHTTP RESTClient)
  add_test(testHTTP testHTTP)
endif(${BUILD_TESTS})
