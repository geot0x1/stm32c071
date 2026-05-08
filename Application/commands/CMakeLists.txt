add_library(commands STATIC)
target_sources(commands PRIVATE commands.h commands.c)
target_include_directories(commands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(commands PRIVATE settings usb telemetry fan_control stm32cubemx)
