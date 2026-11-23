// Runs a scene and a compiled behaviour on the host, tick by tick, with no
// emulator involved.
//
// The guest runs the same bytecode through the same vm.c and the same anim.c, so
// this is a fast way to check that a behaviour terminates, keeps its stack
// discipline and leaves the world in states the renderer can draw - and to see
// the numbers behind a behaviour's tuning without recording a demo.
//
// Build and run from the project root:
//
//   gcc -O2 -std=c99 -I engine/include -o /tmp/behaviour_check \
//       tools/behaviour_check.c engine/src/{anim,scene,state,vm}.c
//   /tmp/behaviour_check bin/MATCH3.SCN bin/MATCH3.BC
//
// The checks below know match-three's conventions - a gem's idle animation is
// its kind plus six, and its glimmer is its kind - but the driver around them is
// the generic part.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/anim.h"
#include "engine/scene.h"
#include "engine/slots.h"
#include "engine/state.h"
#include "engine/vm.h"

#define TICKS (60 * 90)
#define BOARD_CELLS 100
#define KINDS 6
// Handles 0..5 are the glimmer runs and 6..11 the idle ones, one per kind.
#define GLIMMER_BASE 6
// The longest a glimmer should last: its 60 cels, a cel every four ticks, plus
// the quantum an animation started mid-step can wait for.
#define GLIMMER_LIMIT 245

static uint8_t *read_file(const char *path, size_t *size) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  if (length <= 0) {
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

// The keys a player would send to swap two cells. Swaps run often enough that
// they overlap glimmer rolls, which is where the two used to collide over a
// scratch slot.
static int scripted_key(long tick, int *key) {
  static const int arrows[] = {77, 75, 80, 72};
  for (long base = 600, at = 0; base <= 3600; base += 120, at++) {
    if (tick == base) {
      *key = 13; // pick
      return 1;
    }
    if (tick == base + 2) {
      *key = arrows[at % 4];
      return 1;
    }
    if (tick == base + 4) {
      *key = 13; // swap with the pick
      return 1;
    }
  }
  return 0;
}

static int handle_of(const world_state *state, int cell) {
  return state->slot[cell * ENTITY_STRIDE + ENTITY_ANIMATION];
}

static int frame_of(const world_state *state, int cell) {
  return state->slot[cell * ENTITY_STRIDE + ENTITY_FRAME];
}

static int visible_of(const world_state *state, int cell) {
  return state->slot[cell * ENTITY_STRIDE + ENTITY_FLAGS] & ENTITY_VISIBLE;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("usage: behaviour_check <scene> <behaviour.bc>\n");
    return 2;
  }

  scene description;
  world_state state;
  vm machine;
  char error[160];

  if (!scene_load(&description, argv[1], error, sizeof(error))) {
    printf("scene: %s\n", error);
    return 1;
  }
  state_init(&state, &description);

  size_t size = 0;
  uint8_t *bytes = read_file(argv[2], &size);
  if (!bytes) {
    printf("cannot read %s\n", argv[2]);
    return 1;
  }
  if (!vm_load(&machine, bytes, size, state.slot,
               (uint16_t)scene_slot_count(&description), description.seed)) {
    printf("load: %s\n", machine.error ? machine.error : "unknown");
    return 1;
  }
  if (!vm_init(&machine)) {
    printf("init: %s\n", machine.error ? machine.error : "unknown");
    return 1;
  }

  int kind_slot = scene_slot_of(&description, "kind0");
  int phase_slot = scene_slot_of(&description, "phase");
  int dirty_slot = scene_slot_of(&description, "dirty");
  if (kind_slot < 0 || phase_slot < 0 || dirty_slot < 0) {
    printf("the scene is missing a slot this check needs\n");
    return 1;
  }

  long started = 0;
  long concurrent = 0;
  long same_kind = 0;
  long most = 0;
  long glimmers_seen = 0;
  long longest = 0;
  long glimmer_started[BOARD_CELLS];
  for (int cell = 0; cell < BOARD_CELLS; cell++) {
    glimmer_started[cell] = -1;
  }

  for (long tick = 0; tick < TICKS; tick++) {
    // Same order as the engine: step the cels, then input, then the tick.
    if (!anim_advance(&description, &state, (uint32_t)tick, &machine)) {
      printf("tick %ld: animation-end: %s\n", tick,
             machine.error ? machine.error : "unknown");
      return 1;
    }
    int key;
    if (scripted_key(tick, &key) && !vm_key(&machine, key)) {
      printf("tick %ld: key: %s\n", tick, machine.error ? machine.error : "unknown");
      return 1;
    }
    if (!vm_tick(&machine)) {
      printf("tick %ld: %s\n", tick, machine.error ? machine.error : "unknown");
      return 1;
    }

    int glimmering = 0;
    int kinds[BOARD_CELLS];
    for (int cell = 0; cell < BOARD_CELLS; cell++) {
      int handle = handle_of(&state, cell);
      int frame = frame_of(&state, cell);
      int kind = state.slot[kind_slot + cell];
      int visible = visible_of(&state, cell);

      if (handle < 0) {
        if (visible) {
          printf("tick %ld: cell %d has no animation but is visible\n", tick, cell);
          return 1;
        }
        glimmer_started[cell] = -1;
        continue;
      }
      if (handle >= description.sprite_count) {
        printf("tick %ld: cell %d names animation %d of %d\n", tick, cell, handle,
               description.sprite_count);
        return 1;
      }
      if (kind < 0 || kind >= KINDS) {
        printf("tick %ld: cell %d holds kind %d\n", tick, cell, kind);
        return 1;
      }

      // What it is showing has to stay inside the run it names, which is the
      // engine's job and the renderer trusts.
      int frames = description.sprite[handle].frames;
      if (frame < 0 || frame >= frames) {
        printf("tick %ld: cell %d shows cel %d of an animation of %d\n", tick,
               cell, frame, frames);
        return 1;
      }

      if (handle < GLIMMER_BASE) {
        // Glimmering: the handle is the kind, and a glimmer is only ever played
        // on a gem that is on the board.
        if (handle != kind) {
          printf("tick %ld: cell %d glimmers as %d but holds kind %d\n", tick,
                 cell, handle, kind);
          return 1;
        }
        if (!visible) {
          printf("tick %ld: cell %d glimmers while hidden\n", tick, cell);
          return 1;
        }
        if (glimmer_started[cell] < 0) {
          glimmer_started[cell] = tick;
          started++;
        } else if (tick - glimmer_started[cell] > longest) {
          longest = tick - glimmer_started[cell];
        }
        if (tick - glimmer_started[cell] > GLIMMER_LIMIT) {
          printf("tick %ld: cell %d has glimmered for %ld ticks\n", tick, cell,
                 tick - glimmer_started[cell]);
          return 1;
        }
        kinds[glimmering++] = kind;
        continue;
      }

      // Idle: the handle is the kind plus the idle base.
      if (handle != kind + GLIMMER_BASE) {
        printf("tick %ld: cell %d idles as %d but holds kind %d\n", tick, cell,
               handle, kind);
        return 1;
      }
      if (!visible) {
        printf("tick %ld: cell %d has an animation but is not visible\n", tick,
               cell);
        return 1;
      }
      if (glimmer_started[cell] >= 0) {
        glimmers_seen++;
        glimmer_started[cell] = -1;
      }
    }

    if (glimmering > most) {
      most = glimmering;
    }
    if (glimmering >= 2) {
      concurrent++;
    }
    for (int a = 0; a < glimmering; a++) {
      for (int b = a + 1; b < glimmering; b++) {
        if (kinds[a] == kinds[b]) {
          same_kind++;
        }
      }
    }

    // With the board settled, every gem has to be on its own cell. A gem that
    // ended up somewhere else leaves a hole and sits on top of another one, and
    // nothing would ever move it back, because a fall only goes down.
    if (state.slot[phase_slot] == 0 && state.slot[dirty_slot] == 0) {
      for (int cell = 0; cell < BOARD_CELLS; cell++) {
        if (handle_of(&state, cell) < 0) {
          continue;
        }
        int want_x = (40 + 24 * (cell % 10)) << 16;
        int want_y = (24 * (cell / 10)) << 16;
        int x = state.slot[cell * ENTITY_STRIDE + ENTITY_X];
        int y = state.slot[cell * ENTITY_STRIDE + ENTITY_Y];
        if (x != want_x || y != want_y) {
          printf("tick %ld: cell %d is at (%d,%d), not (%d,%d)\n", tick, cell,
                 x >> 16, y >> 16, want_x >> 16, want_y >> 16);
          return 1;
        }
      }
    }
  }

  int hidden = 0;
  for (int cell = 0; cell < BOARD_CELLS; cell++) {
    if (handle_of(&state, cell) < 0) {
      hidden++;
    }
  }
  if (hidden != 0) {
    printf("%d cells are still empty after %d ticks\n", hidden, TICKS);
    return 1;
  }

  printf("ok %d ticks, %ld glimmers started and %ld finished, at most %ld at "
         "once, longest %ld ticks, %ld ticks with two or more, %ld tick-pairs "
         "of one kind, checksum %08lx\n",
         TICKS, started, glimmers_seen, most, longest, concurrent, same_kind,
         (unsigned long)state_checksum(&state, scene_slot_count(&description)));
  return 0;
}
