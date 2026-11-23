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

// Three pages so the page being drawn has been off screen for at least a frame
// by the time we touch it. With two pages the flip has to be timed exactly
// against vertical retrace, and any phase error shows up as tearing; a third
// page removes that coupling. Each page is 19,200 bytes per plane, so three of
// them use 57,600 of the 65,536 bytes available in each plane.
#define PAGE_COUNT 3

#define SCENE_PATH "MATCH3.SCN"
#define LOG_PATH "ENGINE.LOG"

#define KEY_ESCAPE 0x1b
#define KEY_F1 0x3b
#define KEY_F2 0x3c

static void probe_display(void) {
  // Counts how often each status bit is set over ~200 ms. Counting rather than
  // watching for edges matters: a retrace pulse can be far shorter than the
  // polling interval, and an edge watcher samples right past it.
  char line[192];
  uint32_t started = clock_us();
  uint32_t samples = 0;
  uint32_t retrace_high = 0;
  uint32_t blank_high = 0;

  while ((int32_t)(clock_us() - started) < 200000) {
    input_status_1_register status = get_input_status_1();
    if (status.vertical_retrace) {
      retrace_high++;
    }
    if (status.display_enable) {
      blank_high++;
    }
    samples++;
  }

  snprintf(line, sizeof(line),
           "probe samples %lu retrace_high %lu blank_high %lu over %lu us",
           (unsigned long)samples, (unsigned long)retrace_high,
           (unsigned long)blank_high,
           (unsigned long)(clock_us() - started));
  diag_line(line);
}

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

  // Put a finished frame on screen before entering the loop, so the first thing
  // the display shows is never a page that is still being drawn.
  size_t page_index = 0;
  render_frame(&scene_description, &state, 0);
  set_display_start(0);
  page_index = 1;

  uint32_t frame = 0;
  uint32_t last = clock_us();
  uint32_t accumulated = 0;
  int running = 1;

  while (running) {
    uint32_t frame_start = clock_us();
    uint32_t now = frame_start;
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
    size_t page = page_index * render_page_size();
    render_frame(&scene_description, &state, page);
    uint32_t draw_us = clock_us() - render_start;

    set_display_start(page);
    page_index = (page_index + 1) % PAGE_COUNT;
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
        } else if (extended == KEY_F2) {
          probe_display();
        }
        // Other extended keys belong to the language (M1).
      } else if (key == KEY_ESCAPE) {
        running = 0;
      }
    }

    // Pace the loop to the target rate. The flip no longer blocks, so without
    // this the loop free runs at whatever the machine can draw, rendering frames
    // the display never shows.
    uint32_t frame_end = frame_start + FRAME_US;
    while ((int32_t)(clock_us() - frame_end) < 0) {
    }
  }

  diag_state(&scene_description, &state);
  diag_close();
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return 0;
}
