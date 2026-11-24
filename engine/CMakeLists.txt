add_library(engine STATIC
  src/engine.c
  src/anim.c
  src/clock.c
  src/scene.c
  src/state.c
  src/sprite.c
  src/render.c
  src/particles.c
  src/diag.c
  src/vm.c
  src/demo.c
)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE vga common)
