project(RESTClient)

add_subdirectory(base)
add_subdirectory(http)
add_subdirectory(jobManagement)

add_library(RESTClient empty.cpp)
target_link_libraries(RESTClient http jobManagement)
