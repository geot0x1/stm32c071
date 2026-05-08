add_library(watchdog STATIC)

target_sources(watchdog PRIVATE
    watchdog.c
    watchdog.h
)

target_include_directories(watchdog PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(watchdog PRIVATE
    ${CMAKE_SOURCE_DIR}/Application/bsp/iwdg
)

target_link_libraries(watchdog PUBLIC
    bsp_iwdg
)
