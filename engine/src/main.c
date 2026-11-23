#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/nearptr.h>

#include "engine/clock.h"
#include "engine/diag.h"
#include "engine/render.h"
#include "engine/scene.h"
#include "engine/state.h"
#include "engine/vm.h"
#include "vga/vga.h"

#define TARGET_FPS 60
#define FRAME_US (1000000u / TARGET_FPS)
#define MAX_CATCH_UP_US (4u * FRAME_US)

// Two pages, flipped at vertical sync. The flip waits for the retrace edge that
// latches the new start address, so by the time it returns the other page is no
// longer displayed and is safe to draw. The panel starts out as 2 not 3
// deliberately: three pages would allow an unpaced flip but cost 19,200 bytes
// per plane of the sprite space.
#define PAGE_COUNT 2

#define SCENE_PATH "MATCH3.SCN"
#define BEHAVIOUR_PATH "MATCH3.BC"
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

// Reads a whole file into memory. Assets are data on disk, not compiled in, so
// the scene and the behaviour can be replaced without rebuilding.
static uint8_t *read_file(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (length <= 0 || length > 65536) {
    fclose(file);
    return NULL;
  }
  uint8_t *data = malloc((size_t)length);
  if (!data) {
    fclose(file);
    return NULL;
  }
  if (fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    return NULL;
  }
  fclose(file);
  *size = (size_t)length;
  return data;
}

static int behaviour_faulted;

// One call of the behaviour program per key press. A fault is reported once and
// then ignored, so a broken program does not turn every key into a log entry.
static void run_behaviour(vm *machine, world_state *state, int key) {
  if (behaviour_faulted) {
    return;
  }

  char line[128];
  if (!vm_call(machine, key)) {
    behaviour_faulted = 1;
    snprintf(line, sizeof(line), "behaviour fault on key %d: %s", key,
             machine->error ? machine->error : "unknown");
    diag_line(line);
    return;
  }

  snprintf(line, sizeof(line), "key %d checksum %08lx", key,
           (unsigned long)state_checksum(state));
  diag_line(line);
}

int main(void) {
  scene scene_description;
  world_state state;
  vm behaviour;
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

  size_t behaviour_size = 0;
  uint8_t *behaviour_bytes = read_file(BEHAVIOUR_PATH, &behaviour_size);
  bool behaviour_ok =
      behaviour_bytes && vm_load(&behaviour, behaviour_bytes, behaviour_size,
                                 state.slot, SLOT_COUNT);
  if (!behaviour_ok) {
    const char *reason = behaviour_bytes ? behaviour.error : "cannot read it";
    snprintf(error, sizeof(error), "behaviour %s: %s", BEHAVIOUR_PATH,
             reason ? reason : "unknown");
    free(behaviour_bytes);
    __djgpp_nearptr_disable();
    switch_to_mode_3();
    printf("engine: %s\n", error);
    return 1;
  }

  diag_open(LOG_PATH);
  diag_state(&scene_description, &state);

  // Put a finished frame on screen before entering the loop. Draw into the page
  // that is not being displayed (the mode set leaves the display on page 0), so
  // even the first frame is presented complete.
  size_t page_index = 0;
  render_frame(&scene_description, &state, render_page_size());
  set_display_start(render_page_size());
  page_index = 0;

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
        } else {
          run_behaviour(&behaviour, &state, extended);
        }
      } else if (key == KEY_ESCAPE) {
        running = 0;
      } else {
        run_behaviour(&behaviour, &state, key);
      }
    }

    // Pace is supplied by the flip: it returns at the retrace that latches the
    // new page, so the loop runs at the display refresh rate while drawing fits
    // inside one refresh period, and simply free runs when it does not.
  }

  diag_state(&scene_description, &state);
  diag_close();
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return 0;
}
