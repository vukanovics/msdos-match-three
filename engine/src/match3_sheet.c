#include "match3_sheet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "match3_tiles.h"

#define BOX_THICKNESS 2

// A hollow box: the border is opaque, the interior is the transparent key. The
// key is any index the border does not use, so it is chosen per cel.
static void draw_box(uint8_t *pixels, int width, int height, uint8_t colour,
                     uint8_t key) {
  memset(pixels, key, (size_t)width * (size_t)height);
  for (int thickness = 0; thickness < BOX_THICKNESS; thickness++) {
    for (int x = thickness; x < width - thickness; x++) {
      pixels[thickness * width + x] = colour;
      pixels[(height - 1 - thickness) * width + x] = colour;
    }
    for (int y = thickness; y < height - thickness; y++) {
      pixels[y * width + thickness] = colour;
      pixels[y * width + width - 1 - thickness] = colour;
    }
  }
}

static uint8_t key_for(uint8_t colour) { return colour == 0 ? 1 : 0; }

static bool build_match3_sheet(sprite_sheet *sheet, char *error,
                               size_t error_size) {
  int width = MATCH3_TILE_WIDTH;
  int height = MATCH3_TILE_HEIGHT;

  sheet->width = width;
  sheet->height = height;
  sheet->palette = (const uint8_t *)match3_palette;
  sheet->count = MATCH3_TILE_COUNT + 2;

  for (int type = 0; type < MATCH3_TILE_COUNT; type++) {
    // The generated tiles are fully painted, so they come out opaque.
    if (!sprite_cel_build(&sheet->cel[type], match3_tiles[type], width, height,
                          0xff)) {
      snprintf(error, error_size, "tile %d could not be built", type);
      return false;
    }
  }

  uint8_t *pixels = malloc((size_t)width * (size_t)height);
  if (!pixels) {
    snprintf(error, error_size, "out of memory for the cursor cels");
    return false;
  }

  draw_box(pixels, width, height, MATCH3_CURSOR_COLOR,
           key_for(MATCH3_CURSOR_COLOR));
  bool ok = sprite_cel_build(&sheet->cel[MATCH3_TILE_COUNT], pixels, width,
                             height, key_for(MATCH3_CURSOR_COLOR));

  draw_box(pixels, width, height, MATCH3_PICKED_COLOR,
           key_for(MATCH3_PICKED_COLOR));
  ok = ok && sprite_cel_build(&sheet->cel[MATCH3_TILE_COUNT + 1], pixels, width,
                              height, key_for(MATCH3_PICKED_COLOR));
  free(pixels);

  if (!ok) {
    snprintf(error, error_size, "cursor cels could not be built");
  }
  return ok;
}

const sheet_source match3_sheets = {
    .name = "match3_tiles",
    .build = build_match3_sheet,
};
