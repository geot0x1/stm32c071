add_library(telemetry STATIC)

target_sources(telemetry PRIVATE
    telemetry.h
    telemetry.c
)

target_include_directories(telemetry PUBLIC . ..)

target_link_libraries(telemetry PRIVATE
    app_state
    bsp_tim
    bsp_i2c
    temperature_sensor
    pwm_repeater
    fan_control
    settings
    sys_time
    usb
    hdc2010
    push_button
)
