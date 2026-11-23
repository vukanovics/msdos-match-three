add_executable(match3
  src/main.c
  src/clock.c
  src/scene.c
  src/state.c
  src/render.c
  src/diag.c
)
set_target_properties(match3 PROPERTIES SUFFIX ".exe")
target_include_directories(match3 PUBLIC include assets)
target_link_libraries(match3 vga common)

install(TARGETS match3 RUNTIME DESTINATION bin)

# The scene is data, not code: it is installed next to the executable so it can
# be edited and reloaded without rebuilding.
install(FILES assets/match3.scene DESTINATION bin RENAME MATCH3.SCN)
