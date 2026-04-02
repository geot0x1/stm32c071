add_library(ds18b20 STATIC)
target_sources(ds18b20 PRIVATE ds18b20.h ds18b20.c)
target_include_directories(ds18b20 PUBLIC .)
target_link_libraries(ds18b20 PRIVATE onewire)
