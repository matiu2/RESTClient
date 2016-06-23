project(jobManagement)

add_library(jobManagement STATIC JobDistributor.cpp JobRunner.cpp)
target_link_libraries(jobManagement base ${Boost_SYSTEM_LIBRARY} ${Boost_COROUTINE_LIBRARY})
