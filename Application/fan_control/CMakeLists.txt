add_library(fan_control STATIC)
target_sources(fan_control PRIVATE fan_control.h fan_control.c)
target_include_directories(fan_control PUBLIC .)
target_link_libraries(fan_control PUBLIC bsp_tim PRIVATE  bsp_gpio board stm32cubemx)
