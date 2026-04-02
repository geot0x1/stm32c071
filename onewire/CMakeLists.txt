add_library(onewire STATIC)




target_sources(onewire PRIVATE onewire.h onewire.c)
target_include_directories(onewire PUBLIC ./)
target_link_libraries(onewire PUBLIC stm32cubemx critical)

