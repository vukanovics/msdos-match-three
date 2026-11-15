add_library(test_vga test_vga.c ${PROJECT_SOURCE_DIR}/tests/log.c)
target_include_directories(test_vga PUBLIC ${PROJECT_SOURCE_DIR}/include
                                           ${PROJECT_SOURCE_DIR}/tests)
target_link_libraries(test_vga vga)
