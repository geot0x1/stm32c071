add_library(bsp_gpio STATIC
    gpio.c
)

target_include_directories(bsp_gpio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(bsp_gpio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(bsp_gpio PUBLIC
    stm32cubemx
)
