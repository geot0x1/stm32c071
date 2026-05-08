add_library(program_led STATIC)
target_sources(program_led PRIVATE program_led.h program_led.c)
target_include_directories(program_led PUBLIC .)
target_link_libraries(program_led PRIVATE board stm32cubemx)
