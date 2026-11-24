#include "engine/engine.h"

#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/nearptr.h>

#include "engine/anim.h"
#include "engine/clock.h"
#include "engine/diag.h"
#include "engine/render.h"
#include "engine/scene.h"
#include "engine/sprite.h"
#include "engine/state.h"
#include "engine/vm.h"
#include "vga/vga.h"

#define LOG_PATH "ENGINE.LOG"
// Two pages, flipped at vertical sync. The flip waits for the retrace edge that
// latches the new start address, so by the time it returns the other page is no
// longer displayed and is safe to draw. The panel starts out as 2 not 3
// deliberately: three pages would allow an unpaced flip but cost 19,200 bytes
// per plane of the sprite space.
#define PAGE_COUNT 2

// Keys the engine reserves for itself. Everything else is passed to the
// behaviour program as a BIOS key code.
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
static uint32_t logged_checksum;
static bool have_logged_checksum;

// The demo buffers are large and fixed; keeping them off the stack matters
// under DPMI, where the stack is far smaller than the heap.
static demo the_demo;
static particle_system particles;

// A fault is reported once and then ignored, so a broken program does not turn
// every frame into a log entry.
static bool report_fault(bool ok, const vm *machine, const char *what) {
  if (!ok) {
    behaviour_faulted = 1;
    char line[128];
    snprintf(line, sizeof(line), "behaviour %s fault: %s", what,
             machine->error ? machine->error : "unknown");
    diag_line(line);
    return false;
  }
  return true;
}

// The world contains a clock, so its checksum changes on every tick. Hashing it
// every frame costs about a millisecond, and writing a line when it changes
// costs about another, so a per-tick trace is a frame-rate problem rather than a
// debugging aid. State is logged when the program starts and whenever a key
// changes something; F1 dumps it on demand.
static void log_state(const vm *machine, const world_state *state,
                      const scene *scene, const char *what) {
  uint32_t checksum = state_checksum(state, scene_slot_count(scene));
  if (have_logged_checksum && checksum == logged_checksum) {
    return;
  }
  logged_checksum = checksum;
  have_logged_checksum = true;

  char line[128];
  snprintf(line, sizeof(line), "%s checksum %08lx random %08lx", what,
           (unsigned long)checksum, (unsigned long)vm_random_state(machine));
  diag_line(line);
}

// Animation is world state: the engine moves every processed entity's cel along
// and tells the behaviour when a run that does not loop reaches its last cel.
// The stepping itself lives in anim.c, which needs nothing but the scene, so the
// host check runs the same code the guest does.
static void advance_animations(const scene *scene, world_state *state,
                               vm *behaviour) {
  if (!anim_advance(scene, state, behaviour)) {
    report_fault(false, behaviour, "animation-end");
  }
}

int engine_main(const char *scene_path, const char *behaviour_path,
                demo_mode mode, const char *demo_path) {
  scene description;
  world_state state;
  vm behaviour;
  char error[160];
  char demo_summary[128];
  demo_summary[0] = '\0';

  // The sprite set is far too big for the stack under DPMI; it is loaded once
  // and lives for the whole run.
  sprite_set *sprites = malloc(sizeof(sprite_set));
  if (!sprites) {
    printf("engine: out of memory for the sprite set\n");
    return 1;
  }

  switch_to_mode_x();
  __djgpp_nearptr_enable();
  clock_init();

  if (!scene_load(&description, scene_path, error, sizeof(error)) ||
      !sprite_set_load(sprites, &description, error, sizeof(error)) ||
      !render_init(&description, sprites, error, sizeof(error))) {
    __djgpp_nearptr_disable();
    switch_to_mode_3();
    printf("engine: %s\n", error);
    return 1;
  }

  state_init(&state, &description);

  size_t behaviour_size = 0;
  uint8_t *behaviour_bytes = read_file(behaviour_path, &behaviour_size);
  bool behaviour_ok =
      behaviour_bytes &&
      vm_load(&behaviour, behaviour_bytes, behaviour_size, state.slot,
              (uint16_t)scene_slot_count(&description),
              scene_layout_fingerprint(&description), description.seed);
  if (!behaviour_ok) {
    const char *reason = behaviour_bytes ? behaviour.error : "cannot read it";
    snprintf(error, sizeof(error), "behaviour %s: %s", behaviour_path,
             reason ? reason : "unknown");
    free(behaviour_bytes);
    __djgpp_nearptr_disable();
    switch_to_mode_3();
    printf("engine: %s\n", error);
    return 1;
  }

  particles_init(&particles, &description, sprites, &state);
  behaviour.emit_particles = particles_emit;
  behaviour.particle_context = &particles;

  diag_open(LOG_PATH);

  if (!demo_open(&the_demo, mode, demo_path, error, sizeof(error))) {
    __djgpp_nearptr_disable();
    switch_to_mode_3();
    printf("engine: %s\n", error);
    return 1;
  }
  diag_state(&description, &state);
  if (report_fault(vm_init(&behaviour), &behaviour, "init")) {
    log_state(&behaviour, &state, &description, "init");
  }
  // The world as init left it, before the first tick: the starting point every
  // later frame is compared against.
  diag_state(&description, &state);

  // Put a finished frame on screen before entering the loop. Draw into the page
  // that is not being displayed (the mode set leaves the display on page 0), so
  // even the first frame is presented complete.
  size_t page_index = 0;
  render_frame(&description, sprites, &state, &particles, render_page_size());
  set_display_start(render_page_size());

  uint32_t frame = 0;
  int running = 1;

  // Exactly one tick per displayed frame, with input applied at the start of
  // the frame it belongs to. That is what makes frame N mean the same thing on
  // every run: with a clock-derived tick rate, the number of ticks per frame
  // would depend on how fast the host happened to be, and a recording could not
  // be replayed. The cost is that a frame which overruns slows the world down
  // instead of catching up.
  while (running && (mode != DEMO_PLAY || frame < the_demo.frames)) {
    uint32_t frame_start = clock_us();
    uint8_t keys[DEMO_KEYS_PER_FRAME];
    int key_count = 0;
    particles_advance(&particles);

    // Step the cels before anything reads the world, so the tick, the demo hash
    // and the frame that is drawn all describe the same instant.
    if (!behaviour_faulted) {
      advance_animations(&description, &state, &behaviour);
    }

    if (the_demo.mode == DEMO_PLAY) {
      // Only the engine's own keys come from the keyboard during playback.
      if (kbhit()) {
        int key = getch();
        if (key == 0) {
          (void)getch();
        } else if (key == KEY_ESCAPE) {
          running = 0;
        }
      }
      key_count = demo_keys_at(&the_demo, frame, keys, DEMO_KEYS_PER_FRAME);
    } else {
      while (key_count < DEMO_KEYS_PER_FRAME && kbhit()) {
        int key = getch();
        if (key == 0) {
          int extended = getch();
          if (extended == KEY_F1) {
            // Inspection hook: dump the world and timings for the host tools.
            diag_state(&description, &state);
            diag_timings();
          } else if (extended == KEY_F2) {
            probe_display();
          } else {
            keys[key_count++] = (uint8_t)extended;
          }
        } else if (key == KEY_ESCAPE) {
          running = 0;
        } else {
          keys[key_count++] = (uint8_t)key;
        }
      }
    }

    for (int i = 0; i < key_count; i++) {
      if (the_demo.mode == DEMO_RECORD) {
        demo_record_key(&the_demo, frame, keys[i]);
      }
      if (!behaviour_faulted) {
        if (report_fault(vm_key(&behaviour, keys[i]), &behaviour, "key")) {
          log_state(&behaviour, &state, &description, "key");
        }
      }
    }

    uint32_t tick_start = clock_us();
    if (!behaviour_faulted) {
      report_fault(vm_tick(&behaviour), &behaviour, "tick");
    }
    uint32_t tick_us = clock_us() - tick_start;

    // Only a demo needs a checksum per frame; hashing the world costs a fifth
    // of a millisecond and running without one is the normal case.
    if (the_demo.mode != DEMO_OFF) {
      uint32_t hash = state_checksum(&state, scene_slot_count(&description));
      if (the_demo.mode == DEMO_RECORD) {
        demo_record_hash(&the_demo, frame, hash);
      } else {
        demo_check_hash(&the_demo, frame, hash);
      }
    }

    uint32_t render_start = clock_us();
    size_t page = page_index * render_page_size();
    render_frame(&description, sprites, &state, &particles, page);
    uint32_t draw_us = clock_us() - render_start;

    set_display_start(page);
    page_index = (page_index + 1) % PAGE_COUNT;
    uint32_t presented = clock_us();
    uint32_t frame_us = presented - frame_start;
    diag_frame(frame, draw_us, frame_us, presented);

    frame++;
    // Writing the log reopens the file, which is a millisecond or more and lands
    // as a dropped frame. Once every five seconds is enough to watch the rate;
    // the running min/max above still sees every frame.
    if (frame % 300 == 0) {
      render_stats stats;
      render_last_stats(&stats);

      char line[192];
      snprintf(line, sizeof(line),
               "frame %lu draw_us %lu clear_us %lu sprites_us %lu decodes %lu "
               "flip_us %lu tick_us %lu frame_us %lu pit %u clock_us %lu",
               (unsigned long)frame, (unsigned long)draw_us,
               (unsigned long)stats.clear_us, (unsigned long)stats.sprites_us,
               (unsigned long)stats.decodes,
               (unsigned long)(presented - render_start - draw_us),
               (unsigned long)tick_us, (unsigned long)frame_us,
               (unsigned)clock_raw_counter(), (unsigned long)clock_us());
      diag_line(line);
    }

    // Pace is supplied by the flip: it returns at the retrace that latches the
    // new page, so the loop runs at the display refresh rate while drawing fits
    // inside one refresh period, and simply free runs when it does not.
  }

  diag_state(&description, &state);
  if (mode == DEMO_PLAY && frame != the_demo.frames) {
    snprintf(demo_summary, sizeof(demo_summary),
             "demo %s: stopped after %lu of %lu frames", demo_path,
             (unsigned long)frame, (unsigned long)the_demo.frames);
  } else {
    demo_close(&the_demo, demo_summary, sizeof(demo_summary));
  }
  if (demo_summary[0]) {
    diag_line(demo_summary);
  }
  diag_close();
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  if (demo_summary[0]) {
    printf("engine: %s\n", demo_summary);
  }
  return behaviour_faulted ||
         (mode == DEMO_PLAY &&
          (frame != the_demo.frames || the_demo.mismatch_frame != UINT32_MAX));
}
