add_library(pwm_repeater STATIC)

target_sources(pwm_repeater PRIVATE
    pwm_repeater.h
    pwm_repeater.c
)

target_include_directories(pwm_repeater PUBLIC .)

target_link_libraries(pwm_repeater PRIVATE bsp_tim stm32cubemx)
