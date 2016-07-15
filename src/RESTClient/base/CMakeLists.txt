project(base)

add_library(base logger.cpp)
target_link_libraries(base ${CPP} ${CMAKE_THREAD_LIBS_INIT})

if (${BUILD_TESTS})
  add_executable(testURL testURL.cpp)
  target_link_libraries(testURL ${CPP} ${Boost_SYSTEM_LIBRARY})
endif()
