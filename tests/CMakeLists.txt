add_executable(tests main.c)
set_target_properties(tests PROPERTIES SUFFIX ".exe")
install(TARGETS tests RUNTIME DESTINATION bin)

target_link_libraries(tests test_vga)
add_subdirectory(vga)
