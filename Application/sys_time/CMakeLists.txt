add_library(sys_time sys_time.c)
target_include_directories(sys_time PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sys_time PRIVATE stm32cubemx)
