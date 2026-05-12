add_library(commands STATIC)
target_sources(commands PRIVATE commands.h commands.c)
target_include_directories(commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(commands PRIVATE app_state settings usb telemetry fan_control pwm_repeater stm32cubemx fifo)
