add_library(delay STATIC)
target_sources(delay PRIVATE delay.h delay.c)
target_include_directories(delay PUBLIC .)
target_link_libraries(delay PRIVATE stm32cubemx bsp_tim)
