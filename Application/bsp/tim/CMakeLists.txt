add_library(bsp_tim STATIC)

target_sources(bsp_tim PRIVATE
    tim.c
    tim.h
)

target_include_directories(bsp_tim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(bsp_tim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(bsp_tim PUBLIC
    stm32cubemx
)
