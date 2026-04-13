add_library(internal_temp_sensor STATIC
)
target_sources(internal_temp_sensor PRIVATE
internal_temp_sensor.c
internal_temp_sensor.h
)

target_include_directories(internal_temp_sensor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(internal_temp_sensor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(internal_temp_sensor PRIVATE
    bsp_adc
    stm32cubemx
)
