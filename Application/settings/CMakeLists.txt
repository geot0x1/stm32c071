add_library(settings STATIC settings.c)
target_include_directories(settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(settings PRIVATE flash crc32)
