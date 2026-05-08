add_library(timers STATIC
    timers.c
)

target_include_directories(timers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(timers PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
    ${CMAKE_SOURCE_DIR}/Application/bsp/tim
    ${CMAKE_SOURCE_DIR}/Application/bsp/gpio
)

target_link_libraries(timers PUBLIC
    bsp_tim
    bsp_gpio
    stm32cubemx
)
