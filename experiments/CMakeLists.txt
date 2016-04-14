project (experiments)

add_executable(asio asio.cpp)
target_link_libraries(asio ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES})
