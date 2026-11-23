#include <conio.h>
#include <stdio.h>
#include <sys/nearptr.h>

#include "engine/clock.h"
#include "engine/diag.h"
#include "engine/render.h"
#include "engine/scene.h"
#include "engine/state.h"
#include "vga/vga.h"

#define TARGET_FPS 60
#define FRAME_US (1000000u / TARGET_FPS)
#define MAX_CATCH_UP_US (4u * FRAME_US)

#define SCENE_PATH "MATCH3.SCN"
#define LOG_PATH "ENGINE.LOG"

#define KEY_ESCAPE 0x1b
#define KEY_F1 0x3b

int main(void) {
  scene scene_description;
  world_state state;
  char error[160];

  switch_to_mode_x();
  __djgpp_nearptr_enable();
  clock_init();

  if (!scene_load(&scene_description, SCENE_PATH, error, sizeof(error)) ||
      !render_init(&scene_description, error, sizeof(error))) {
    __djgpp_nearptr_disable();
    switch_to_mode_3();
    printf("engine: %s\n", error);
    return 1;
  }

  state_init(&state, &scene_description);
  diag_open(LOG_PATH);
  diag_state(&scene_description, &state);

  size_t page = 0;
  uint32_t frame = 0;
  uint32_t last = clock_us();
  uint32_t accumulated = 0;
  int running = 1;

  while (running) {
    uint32_t now = clock_us();
    uint32_t delta = now - last;
    last = now;
    if (delta > MAX_CATCH_UP_US) {
      delta = MAX_CATCH_UP_US;
    }

    // Fixed step: the world advances in whole 1/60 s ticks no matter how long
    // drawing took. M1 runs the compiled (state, input) -> state step here.
    accumulated += delta;
    while (accumulated >= FRAME_US) {
      accumulated -= FRAME_US;
    }

    uint32_t render_start = clock_us();
    render_frame(&scene_description, &state, page);
    uint32_t draw_us = clock_us() - render_start;

    set_display_start(page);
    page = (page == 0) ? render_page_size() : 0;
    uint32_t frame_us = clock_us() - render_start;

    frame++;
    if (frame % 60 == 0) {
      render_stats stats;
      render_last_stats(&stats);

      char line[192];
      snprintf(line, sizeof(line),
               "frame %lu draw_us %lu clear_us %lu tiles_us %lu border_us "
               "%lu flip_us %lu tick %lu pit %u clock_us %lu",
               (unsigned long)frame, (unsigned long)draw_us,
               (unsigned long)stats.clear_us, (unsigned long)stats.tiles_us,
               (unsigned long)stats.border_us,
               (unsigned long)(frame_us - draw_us),
               (unsigned long)clock_raw_tick(),
               (unsigned)clock_raw_counter(), (unsigned long)clock_us());
      diag_line(line);
      diag_frame(frame, draw_us, frame_us, clock_us());
    }

    if (kbhit()) {
      int key = getch();
      if (key == 0) {
        int extended = getch();
        if (extended == KEY_F1) {
          // Inspection hook: dump the world and the timings for the host tools.
          diag_state(&scene_description, &state);
          diag_timings();
        }
        // Other extended keys belong to the language (M1).
      } else if (key == KEY_ESCAPE) {
        running = 0;
      }
    }

    // No explicit pacing: the vsync latched page flip already paces the loop at
    // the refresh rate, and busy waiting would only invite the emulator to
    // throttle the emulated CPU.
  }

  diag_state(&scene_description, &state);
  diag_close();
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return 0;
}
