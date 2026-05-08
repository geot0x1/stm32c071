add_library(bsp_usb STATIC)

target_sources(bsp_usb PRIVATE
    bsp_usb.c
    bsp_usb.h
)

target_include_directories(bsp_usb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(bsp_usb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(bsp_usb PUBLIC
    stm32cubemx
)
