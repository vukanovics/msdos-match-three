add_executable(mtchthre main.c)
set_target_properties(mtchthre PROPERTIES SUFFIX ".exe")
install(TARGETS mtchthre RUNTIME DESTINATION bin)

target_link_libraries(mtchthre vga)
target_include_directories(mtchthre PUBLIC ${PROJECT_SOURCE_DIR}/include)
add_subdirectory(vga)
