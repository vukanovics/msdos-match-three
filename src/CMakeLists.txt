add_executable(mtchthre main.c)
set_target_properties(mtchthre PROPERTIES SUFFIX ".exe")
install(TARGETS mtchthre RUNTIME DESTINATION bin)
