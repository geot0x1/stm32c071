add_library(push_button STATIC)
target_sources(push_button PRIVATE push_button.h push_button.c)
target_include_directories(push_button PUBLIC .)
target_link_libraries(push_button PRIVATE board stm32cubemx)
