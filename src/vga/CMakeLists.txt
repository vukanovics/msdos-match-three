add_library(vga vga.c planarized_image.c)
target_include_directories(vga PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(vga common)
