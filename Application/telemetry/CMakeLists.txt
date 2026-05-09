add_library(telemetry STATIC)

target_sources(telemetry PRIVATE
    telemetry.h
    telemetry.c
)

target_include_directories(telemetry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(telemetry PRIVATE
    bsp_tim
    bsp_i2c
    temperature_sensor
    pwm_repeater
    fan_control
    settings
    sys_time
    usb
    hdc2010
)
