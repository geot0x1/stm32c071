add_library(pwm_repeater STATIC)

target_sources(pwm_repeater PRIVATE
    pwm_repeater.h
    pwm_repeater.c
)

target_include_directories(pwm_repeater PUBLIC .)

target_link_libraries(pwm_repeater PUBLIC sys_time PRIVATE bsp_tim bsp_gpio critical stm32cubemx)
