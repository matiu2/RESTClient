project(http)

add_library(http STATIC HTTP.cpp HTTPBody.cpp Services.cpp)
target_link_libraries(http base ${Boost_SYSTEM_LIBRARY} ${Boost_IOSTREAMS_LIBRARY} ${OPENSSL_LIBRARIES})
