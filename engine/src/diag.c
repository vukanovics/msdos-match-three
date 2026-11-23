#include "engine/diag.h"

#include <stdio.h>

static char log_path[64];

static uint32_t frames;
static uint32_t draw_min_us = 0xffffffffu;
static uint32_t draw_max_us;
static uint64_t draw_total_us;
static uint32_t frame_max_us;

void diag_open(const char *path) {
  snprintf(log_path, sizeof(log_path), "%s", path);

  // Truncate once; later writes append.
  FILE *file = fopen(log_path, "w");
  if (file) {
    fclose(file);
  }
}

static FILE *append(void) { return log_path[0] ? fopen(log_path, "a") : NULL; }

void diag_line(const char *text) {
  FILE *file = append();
  if (!file) {
    return;
  }
  fprintf(file, "%s\n", text);
  fclose(file);
}

void diag_frame(uint32_t frame, uint32_t draw_us, uint32_t frame_us,
                uint32_t now_us) {
  (void)frame;
  (void)now_us;

  frames++;
  draw_total_us += draw_us;
  if (draw_us < draw_min_us) {
    draw_min_us = draw_us;
  }
  if (draw_us > draw_max_us) {
    draw_max_us = draw_us;
  }
  if (frame_us > frame_max_us) {
    frame_max_us = frame_us;
  }
}

void diag_timings(void) {
  if (!frames) {
    return;
  }

  char line[160];
  snprintf(line, sizeof(line),
           "timings frames %lu draw_avg_us %lu draw_min_us %lu "
           "draw_max_us %lu frame_max_us %lu",
           (unsigned long)frames, (unsigned long)(draw_total_us / frames),
           (unsigned long)draw_min_us, (unsigned long)draw_max_us,
           (unsigned long)frame_max_us);
  diag_line(line);
}

void diag_state(const scene *scene, const world_state *state) {
  FILE *file = append();
  if (!file) {
    return;
  }

  char description[128];
  scene_describe(scene, description, sizeof(description));

  fprintf(file, "state %s\n", description);
  fprintf(file, "  checksum %08lx\n",
          (unsigned long)state_checksum(state, scene_slot_count(scene)));

  // The entity pool, in the engine's own terms: a sprite at a position. Only
  // the visible ones are worth a line.
  for (int entity = 0; entity < scene->entity_count; entity++) {
    const int32_t *slots = &state->slot[entity * ENTITY_STRIDE];
    if (!(slots[ENTITY_FLAGS] & ENTITY_VISIBLE)) {
      continue;
    }
    fprintf(file,
            "  entity %3d anim %2ld frame %2ld x %ld y %ld flags %02lx\n",
            entity, (long)slots[ENTITY_ANIMATION], (long)slots[ENTITY_FRAME],
            (long)(slots[ENTITY_X] >> ENTITY_FIXED_SHIFT),
            (long)(slots[ENTITY_Y] >> ENTITY_FIXED_SHIFT),
            (unsigned long)(slots[ENTITY_FLAGS] & 0xffffL));
  }

  int base = scene->entity_count * ENTITY_STRIDE;
  for (int i = 0; i < scene->global_count; i++) {
    fprintf(file, "  slot %-16s %ld\n", scene->global[i].name,
            (long)state->slot[base + i]);
  }
  fclose(file);
}

void diag_close(void) {
  diag_timings();
  log_path[0] = '\0';
}
