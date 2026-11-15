add_library(game game.c)
target_include_directories(game PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(game vga common)
