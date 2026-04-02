add_library(critical critical.c)
target_include_directories(critical PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(critical PRIVATE stm32cubemx)
