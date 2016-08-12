project (src)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(RESTClient)

if (${BUILD_TESTS})
  # This executable tests our HTTP interface against http://httpbin.org
  add_executable(testHTTP testHTTP.cpp)
  target_link_libraries(testHTTP RESTClient ${Boost_REGEX_LIBRARY})
  add_test(testHTTP testHTTP)
  if (${BUILD_RS_TESTS})
    # Tests zipped and chunked uploads against the Rackspace cloudfiles API
    # Requires settings RS_USERNAME, RS_APIKEY, and RS_REGION
    add_executable(testRackspaceAPI testRackspaceAPI.cpp)
    target_link_libraries(testRackspaceAPI RESTClient ${JSON_LIB} ${Boost_REGEX_LIBRARY})
    add_test(testHTTP testHTTP)
  endif()
endif(${BUILD_TESTS})
