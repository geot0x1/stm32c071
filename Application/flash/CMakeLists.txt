add_library(flash STATIC flash.c)
target_include_directories(flash PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flash PRIVATE stm32cubemx)
