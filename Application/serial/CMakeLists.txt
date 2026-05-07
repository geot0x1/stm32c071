add_library(serial STATIC)

target_sources(serial PRIVATE
    serial.c
    serial.h
)

target_include_directories(serial PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(serial PRIVATE
    ${CMAKE_SOURCE_DIR}/Application/bsp/uart
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(serial
    PRIVATE bsp_uart
    PUBLIC  stm32cubemx
)
