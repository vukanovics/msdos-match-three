add_executable(match3
  src/main.c
  src/clock.c
  src/scene.c
  src/state.c
  src/render.c
  src/diag.c
  src/vm.c
)
set_target_properties(match3 PROPERTIES SUFFIX ".exe")
target_include_directories(match3 PUBLIC include assets)
target_link_libraries(match3 vga common)

install(TARGETS match3 RUNTIME DESTINATION bin)

# The scene is data, not code: it is installed next to the executable so it can
# be edited and reloaded without rebuilding.
install(FILES assets/match3.scene DESTINATION bin RENAME MATCH3.SCN)

# The behaviour language is compiled on the host, so type, name and termination
# problems are build errors; the guest only ever runs bytecode. The compiler is
# a dependency-free Rust program, so it builds offline.
set(BEHAVIOURC_DIR ${PROJECT_SOURCE_DIR}/tools/behaviourc)
set(BEHAVIOURC_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/behaviourc-build)
set(BEHAVIOURC ${BEHAVIOURC_TARGET_DIR}/release/behaviourc)

add_custom_command(
  OUTPUT ${BEHAVIOURC}
  COMMAND cargo build --release --offline
          --manifest-path ${BEHAVIOURC_DIR}/Cargo.toml
          --target-dir ${BEHAVIOURC_TARGET_DIR}
  DEPENDS ${BEHAVIOURC_DIR}/src/main.rs ${BEHAVIOURC_DIR}/Cargo.toml
  COMMENT "Building the behaviour compiler"
  VERBATIM
)

set(BEHAVIOUR ${CMAKE_CURRENT_BINARY_DIR}/MATCH3.BC)
add_custom_command(
  OUTPUT ${BEHAVIOUR}
  COMMAND ${BEHAVIOURC}
          ${CMAKE_CURRENT_SOURCE_DIR}/assets/match3.slots
          ${CMAKE_CURRENT_SOURCE_DIR}/assets/match3.behaviour
          ${BEHAVIOUR}
  DEPENDS ${BEHAVIOURC}
          ${CMAKE_CURRENT_SOURCE_DIR}/assets/match3.slots
          ${CMAKE_CURRENT_SOURCE_DIR}/assets/match3.behaviour
  COMMENT "Compiling match3.behaviour"
  VERBATIM
)

add_custom_target(behaviour DEPENDS ${BEHAVIOUR})
add_dependencies(match3 behaviour)
install(FILES ${BEHAVIOUR} DESTINATION bin)
