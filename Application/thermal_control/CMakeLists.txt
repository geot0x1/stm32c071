add_library(thermal_control STATIC)
target_sources(thermal_control PRIVATE thermal_control.h thermal_control.c)
target_include_directories(thermal_control PUBLIC . PRIVATE ..)
target_link_libraries(thermal_control PRIVATE settings system_temp)
