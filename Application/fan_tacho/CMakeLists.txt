add_library(fan_tacho STATIC)
target_sources(fan_tacho PRIVATE fan_tacho.h fan_tacho.c)
target_include_directories(fan_tacho PUBLIC .)
target_link_libraries(fan_tacho PRIVATE bsp_gpio board stm32cubemx)
