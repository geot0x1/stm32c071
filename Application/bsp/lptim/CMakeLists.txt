# NOTE: bsp_lptim requires stm32c0xx_hal_lptim.c/.h from the STM32CubeC0 package.
# See Application/bsp/lptim/lptim.h for the full list of prerequisites before
# enabling this target. Once HAL files are present and HAL_LPTIM_MODULE_ENABLED
# is defined, uncomment add_subdirectory(lptim) in the parent CMakeLists.txt
# and add bsp_lptim to the executable's target_link_libraries.

add_library(bsp_lptim STATIC
    lptim.c
)

target_include_directories(bsp_lptim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(bsp_lptim PRIVATE
    ${CMAKE_SOURCE_DIR}/Core/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/STM32C0xx_HAL_Driver/Inc
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Include
    ${CMAKE_SOURCE_DIR}/Drivers/CMSIS/Device/ST/STM32C0xx/Include
)

target_link_libraries(bsp_lptim PUBLIC
    stm32cubemx
)
