project (experiments)

add_executable(asio asio.cpp)
target_link_libraries(asio ${CPP} ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES}
                                 ${Boost_SYSTEM_LIBRARY}
                                 ${Boost_COROUTINE_LIBRARY} ${Boost_IOSTREAMS_LIBRARY})
