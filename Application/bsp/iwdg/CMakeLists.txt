add_library(bsp_iwdg STATIC)

target_sources(bsp_iwdg PRIVATE
    iwdg.c
    iwdg.h
)

target_include_directories(bsp_iwdg PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(bsp_iwdg PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(bsp_iwdg PUBLIC
    stm32cubemx
)
