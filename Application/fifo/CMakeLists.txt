add_library(fifo STATIC)

target_sources(fifo PRIVATE
    fifo.c
    fifo.h
)

target_include_directories(fifo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
