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
  fprintf(file, "  checksum %08lx cursor %d,%d picked %d,%d random %08lx\n",
          (unsigned long)state_checksum(state), state->cursor_row,
          state->cursor_column, state->picked_row, state->picked_column,
          (unsigned long)state->random);

  for (int row = 0; row < scene->rows; row++) {
    fprintf(file, "  row %2d:", row);
    for (int column = 0; column < scene->columns; column++) {
      fprintf(file, " %2d", state->tiles[row][column]);
    }
    fprintf(file, "\n");
  }
  fclose(file);
}

void diag_close(void) {
  diag_timings();
  log_path[0] = '\0';
}
