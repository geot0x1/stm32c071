add_library(app_state STATIC)

target_sources(app_state PRIVATE
    app_state.h
    app_state.c
)

target_include_directories(app_state PUBLIC .)
