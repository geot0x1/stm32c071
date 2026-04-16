add_library(board STATIC
    board.c
)

target_include_directories(board PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(board PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
    ${CMAKE_SOURCE_DIR}/Application/bsp/gpio
    ${CMAKE_SOURCE_DIR}/Application/bsp/i2c
    ${CMAKE_SOURCE_DIR}/Application/bsp/usb
    ${CMAKE_SOURCE_DIR}/Application/bsp/uart
)

target_link_libraries(board PUBLIC
    bsp_gpio
    bsp_i2c
    bsp_usb
    bsp_uart
    stm32cubemx
)
