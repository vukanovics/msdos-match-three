#include "engine/render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/nearptr.h>

#include "engine/clock.h"
#include "match3_tiles.h"
#include "vga/vga.h"

// Per-plane bytes for one screen page.
#define PAGE_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT) / 4)

// The flip writes only the start address high byte, which is safe only while
// every page base is a multiple of 256 (see set_display_start). Fail the build
// rather than the display if the mode or page layout ever changes.
typedef char page_size_must_be_a_multiple_of_256[(PAGE_SIZE % 256 == 0) ? 1 : -1];

// Bytes per scanline in one plane.
#define ROW_BYTES (SCREEN_WIDTH / 4)

#define BORDER_THICKNESS 2

static planarized_image *tiles[MATCH3_TILE_COUNT];
static int origin_x;
static int origin_y;
static render_stats last_stats;

void render_last_stats(render_stats *stats) { *stats = last_stats; }

static uint8_t *video_memory(void) {
  return (uint8_t *)(0xa0000 + __djgpp_conventional_base);
}

static planarized_image *make_tile(int type) {
  size tile_size = {.w = MATCH3_TILE_WIDTH, .h = MATCH3_TILE_HEIGHT};
  size_t bytes = MATCH3_TILE_WIDTH * MATCH3_TILE_HEIGHT;

  // One-off allocation at load time; nothing is allocated per frame.
  uint8_t *data = malloc(bytes);
  memcpy(data, match3_tiles[type], bytes);

  return planarized_image_new(image_new(data, tile_size));
}

size_t render_page_size(void) { return PAGE_SIZE; }

bool render_init(const scene *scene, char *error, size_t error_size) {
  if (scene->sprite_count != MATCH3_TILE_COUNT ||
      scene->sprite_width != MATCH3_TILE_WIDTH ||
      scene->sprite_height != MATCH3_TILE_HEIGHT) {
    snprintf(error, error_size,
             "scene sprites %d@%dx%d do not match the built-in sheet %d@%dx%d",
             scene->sprite_count, scene->sprite_width, scene->sprite_height,
             MATCH3_TILE_COUNT, MATCH3_TILE_WIDTH, MATCH3_TILE_HEIGHT);
    return false;
  }
  if (scene->tile_size != MATCH3_TILE_WIDTH) {
    snprintf(error, error_size,
             "board.tile must be %d; scaling is not supported yet",
             MATCH3_TILE_WIDTH);
    return false;
  }

  set_palette((const uint8_t *)match3_palette, 256);
  set_gc(&GC_BIT_MASK, 0xff);

  for (int type = 0; type < MATCH3_TILE_COUNT; type++) {
    tiles[type] = make_tile(type);
  }

  origin_x = (SCREEN_WIDTH - scene->columns * scene->tile_size) / 2;
  origin_y = (SCREEN_HEIGHT - scene->rows * scene->tile_size) / 2;
  return true;
}

// Clearing touches every byte of one plane before moving to the next, so the
// sequencer mask is programmed four times instead of once per scanline.
static void clear_page(uint8_t *vram, size_t base, uint8_t color) {
  for (int plane = 0; plane < 4; plane++) {
    set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));
    memset(vram + base, color, PAGE_SIZE);
  }
}

// Mode X writes are gated by the sequencer map mask, and changing that mask is
// by far the most expensive part of a blit. Grouping the work by plane means
// four mask changes per sprite rather than one per scanline per plane.
static void blit_tile(planarized_image *image, int x, int y, size_t page_base) {
  uint8_t *planes[4] = {
      planarized_image_get_plane_1_data(image),
      planarized_image_get_plane_2_data(image),
      planarized_image_get_plane_3_data(image),
      planarized_image_get_plane_4_data(image),
  };
  uint8_t *vram = video_memory();
  int width_per_plane = MATCH3_TILE_WIDTH / 4;
  size_t column = (size_t)(x / 4);

  for (int plane = 0; plane < 4; plane++) {
    set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));
    const uint8_t *source = planes[plane];
    for (int row = 0; row < MATCH3_TILE_HEIGHT; row++) {
      size_t destination = page_base + (size_t)(y + row) * ROW_BYTES + column;
      memcpy(vram + destination, source + row * width_per_plane,
             width_per_plane);
    }
  }
}

// Same idea for the cursor: walk each plane once and write only the pixels that
// belong to it.
static void outline(uint8_t *vram, size_t page_base, int x, int y, int width,
                    int height, uint8_t color) {
  for (int plane = 0; plane < 4; plane++) {
    set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));

    for (int thickness = 0; thickness < BORDER_THICKNESS; thickness++) {
      for (int i = 0; i < width; i++) {
        int pixel = x + i;
        if ((pixel & 3) != plane) {
          continue;
        }
        vram[page_base + (size_t)(y + thickness) * ROW_BYTES + (pixel >> 2)] =
            color;
        vram[page_base + (size_t)(y + height - 1 - thickness) * ROW_BYTES +
             (pixel >> 2)] = color;
      }
      for (int i = 0; i < height; i++) {
        int pixel = x + thickness;
        if ((pixel & 3) == plane) {
          vram[page_base + (size_t)(y + i) * ROW_BYTES + (pixel >> 2)] = color;
        }
        pixel = x + width - 1 - thickness;
        if ((pixel & 3) == plane) {
          vram[page_base + (size_t)(y + i) * ROW_BYTES + (pixel >> 2)] = color;
        }
      }
    }
  }
}

static void draw_borders(const scene *scene, const world_state *state,
                         uint8_t *vram, size_t page_base) {
  int x = origin_x + state->cursor_column * scene->tile_size;
  int y = origin_y + state->cursor_row * scene->tile_size;
  int size = scene->tile_size;

  if (state->picked_row == state->cursor_row &&
      state->picked_column == state->cursor_column) {
    // Both land on the same tile; the pick colour wins so it stays visible.
    outline(vram, page_base, x, y, size, size, scene->picked);
    return;
  }

  if (state->picked_row >= 0) {
    outline(vram, page_base, origin_x + state->picked_column * scene->tile_size,
            origin_y + state->picked_row * scene->tile_size, size, size,
            scene->picked);
  }
  outline(vram, page_base, x, y, size, size, scene->cursor);
}

void render_frame(const scene *scene, const world_state *state,
                  size_t page_base) {
  uint8_t *vram = video_memory();
  uint32_t start = clock_us();

  set_draw_page(page_base);
  clear_page(vram, page_base, scene->background);
  uint32_t after_clear = clock_us();

  for (int row = 0; row < scene->rows; row++) {
    for (int column = 0; column < scene->columns; column++) {
      int type = state->tiles[row][column];
      if (type < 0 || type >= MATCH3_TILE_COUNT) {
        continue;
      }
      blit_tile(tiles[type], origin_x + column * scene->tile_size,
                origin_y + row * scene->tile_size, page_base);
    }
  }
  uint32_t after_tiles = clock_us();

  draw_borders(scene, state, vram, page_base);
  uint32_t after_borders = clock_us();

  last_stats.clear_us = after_clear - start;
  last_stats.tiles_us = after_tiles - after_clear;
  last_stats.border_us = after_borders - after_tiles;
}
