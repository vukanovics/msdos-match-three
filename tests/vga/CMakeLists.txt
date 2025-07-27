add_library(test_vga test_vga.c)
target_include_directories(test_vga PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(test_vga vga)
