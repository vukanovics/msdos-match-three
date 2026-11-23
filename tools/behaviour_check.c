// Runs a scene and a compiled behaviour on the host, tick by tick, with no
// emulator involved.
//
// The guest runs the same bytecode through the same vm.c, so this is a fast way
// to check that a behaviour terminates, keeps its stack discipline and leaves
// the world in states the renderer can draw - and to see the numbers behind a
// behaviour's tuning without recording a demo.
//
// Build and run from the project root:
//
//   gcc -O2 -std=c99 -I engine/include -o /tmp/behaviour_check \
//       tools/behaviour_check.c engine/src/{scene,state,vm}.c
//   /tmp/behaviour_check bin/MATCH3.SCN bin/MATCH3.BC
//
// The checks below are specific to match-three's glimmer pool; the driver around
// them is the generic part.

#include <stdio.h>
#include <stdlib.h>

#include "engine/scene.h"
#include "engine/slots.h"
#include "engine/state.h"
#include "engine/vm.h"

#define TICKS (60 * 90)
#define BOARD_CELLS 100
#define GLIMMER_SLOTS 4

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

static int handle_of(const world_state *state, int cell) {
  return state->slot[cell * ENTITY_STRIDE + ENTITY_SPRITE];
}

// The keys a player would send to swap two cells, at the ticks given.
static int scripted_key(long tick, int *key) {
  if (tick == 600) {
    *key = 13; // pick
    return 1;
  }
  if (tick == 602) {
    *key = 77; // right
    return 1;
  }
  if (tick == 604) {
    *key = 13; // swap with the pick
    return 1;
  }
  if (tick == 1800) {
    *key = 80; // down
    return 1;
  }
  if (tick == 1802) {
    *key = 13;
    return 1;
  }
  if (tick == 1804) {
    *key = 13;
    return 1;
  }
  return 0;
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

  int count_slot = scene_slot_of(&description, "glimmer-count");
  int cell0_slot = scene_slot_of(&description, "glimmer-cell0");
  if (count_slot < 0 || cell0_slot < 0) {
    printf("the scene has no glimmer pool\n");
    return 1;
  }

  long started = 0;
  long concurrent = 0;
  long same_kind = 0;
  long most = 0;
  int previous = 0;

  for (long tick = 0; tick < TICKS; tick++) {
    int key;
    if (scripted_key(tick, &key) && !vm_key(&machine, key)) {
      printf("key: %s\n", machine.error ? machine.error : "unknown");
      return 1;
    }

    if (!vm_tick(&machine)) {
      printf("tick %ld: %s\n", tick, machine.error ? machine.error : "unknown");
      return 1;
    }

    int count = state.slot[count_slot];
    if (count < 0 || count > GLIMMER_SLOTS) {
      printf("tick %ld: the pool holds %d entries\n", tick, count);
      return 1;
    }
    if (count > previous) {
      started += count - previous;
    }
    previous = count;
    if (count > most) {
      most = count;
    }
    if (count >= 2) {
      concurrent++;
    }

    // Every entry names a board cell, and that cell is wearing a glimmer sprite.
    int kinds[GLIMMER_SLOTS];
    for (int i = 0; i < count; i++) {
      int cell = state.slot[cell0_slot + i];
      if (cell < 0 || cell >= BOARD_CELLS) {
        printf("tick %ld: entry %d names cell %d\n", tick, i, cell);
        return 1;
      }
      kinds[i] = handle_of(&state, cell);
      if (kinds[i] < 0 || kinds[i] >= 6) {
        printf("tick %ld: entry %d, cell %d, wears handle %d, not a glimmer\n",
               tick, i, cell, kinds[i]);
        return 1;
      }
    }
    for (int a = 0; a < count; a++) {
      for (int b = a + 1; b < count; b++) {
        if (kinds[a] == kinds[b]) {
          same_kind++;
        }
      }
    }

    // Nothing outside the pool may be wearing a glimmer, because matching and
    // swapping read sprite handles to decide what a gem is. A hidden cell is
    // only legal while the board is not quiet.
    for (int cell = 0; cell < BOARD_CELLS; cell++) {
      int handle = handle_of(&state, cell);
      int in_pool = 0;
      for (int i = 0; i < count; i++) {
        if (state.slot[cell0_slot + i] == cell) {
          in_pool = 1;
        }
      }
      if (in_pool || handle == -1) {
        continue;
      }
      if (handle < 6 || handle >= 12) {
        printf("tick %ld: cell %d wears handle %d outside the pool\n", tick,
               cell, handle);
        return 1;
      }
    }
  }

  printf("ok %d ticks, %ld glimmers started, at most %ld at once, %ld ticks "
         "with two or more, %ld tick-pairs of one kind, checksum %08lx\n",
         TICKS, started, most, concurrent, same_kind,
         (unsigned long)state_checksum(&state, scene_slot_count(&description)));
  return 0;
}
