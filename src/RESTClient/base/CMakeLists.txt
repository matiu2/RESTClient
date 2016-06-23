project(base)

add_library(base logger.cpp)
target_link_libraries(base ${CPP} ${CMAKE_THREAD_LIBS_INIT})
