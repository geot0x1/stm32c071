add_library(usb STATIC)

target_sources(usb PRIVATE
    usb.h
    usb.c
    usb_descriptors.c

    # TinyUSB Core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/tusb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/common/tusb_fifo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/device/usbd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/device/usbd_control.c

    # TinyUSB Classes
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/class/cdc/cdc_device.c

    # TinyUSB Driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c
)

target_include_directories(usb PUBLIC .)

target_include_directories(usb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Inc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tinyusb/src/portable/st/stm32_fsdev
)

target_compile_definitions(usb PRIVATE
    CFG_TUSB_MCU=OPT_MCU_STM32C0
)

target_link_libraries(usb PRIVATE stm32cubemx)
