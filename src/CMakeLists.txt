project (src)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(RESTClient)

if (${BUILD_TESTS})
  add_executable(testHTTP testHTTP.cpp)
  target_link_libraries(testHTTP RESTClient ${Boost_REGEX_LIBRARY})
  add_test(testHTTP testHTTP)
endif(${BUILD_TESTS})
