#include "engine/sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vga/vga.h"

// File layout, matching tools/png_to_spr.py.
#define SHEET_MAGIC "SPR1"
#define SHEET_HEADER_SIZE 10
#define SHEET_PALETTE_OFFSET SHEET_HEADER_SIZE
#define SHEET_TRANSPARENT_OFFSET (SHEET_HEADER_SIZE + SPRITE_PALETTE_BYTES)

// Room for the widest phase: pixel width - 1 shifted by up to 3, then rounded
// up to a whole byte, plus the byte that the shift spills into.
static int bytes_per_row_for(int width) { return ((width + 2) >> 2) + 1; }

static uint8_t *allocate(size_t bytes, bool zeroed) {
  return zeroed ? calloc(bytes, 1) : malloc(bytes);
}

static bool build_plane(sprite_plane *this, const uint8_t *pixels, int width,
                        int height, int phase, int plane, bool opaque,
                        uint8_t transparent, int bytes_per_row) {
  size_t bytes = (size_t)bytes_per_row * (size_t)height;
  this->data = allocate(bytes, true);
  this->present = opaque ? NULL : allocate(bytes, true);
  this->row_first = opaque ? NULL : malloc((size_t)height);
  this->row_last = opaque ? NULL : malloc((size_t)height);
  this->first = 0xff;
  this->last = 0;
  if (!this->data || (!opaque && (!this->present || !this->row_first ||
                                  !this->row_last))) {
    return false;
  }
  if (!opaque) {
    memset(this->row_first, 0xff, (size_t)height);
    memset(this->row_last, 0, (size_t)height);
  }

  for (int row = 0; row < height; row++) {
    for (int column = 0; column < width; column++) {
      uint8_t colour = pixels[row * width + column];
      if (!opaque && colour == transparent) {
        continue;
      }
      // The destination byte and plane of this pixel, in the pre-shifted frame.
      if (((phase + column) & 3) != plane) {
        continue;
      }
      int byte = (phase + column) >> 2;
      size_t at = (size_t)row * (size_t)bytes_per_row + (size_t)byte;
      this->data[at] = colour;
      if (this->present) {
        this->present[at] = 1;
      }
      if ((uint8_t)byte < this->first) {
        this->first = (uint8_t)byte;
      }
      if ((uint8_t)byte > this->last) {
        this->last = (uint8_t)byte;
      }
      if (this->row_first) {
        if ((uint8_t)byte < this->row_first[row]) {
          this->row_first[row] = (uint8_t)byte;
        }
        if ((uint8_t)byte > this->row_last[row]) {
          this->row_last[row] = (uint8_t)byte;
        }
      }
    }
  }
  return true;
}

bool sprite_cel_build(sprite_cel *this, const uint8_t *pixels, int width,
                      int height, uint8_t transparent) {
  memset(this, 0, sizeof(*this));
  this->width = width;
  this->height = height;
  this->bytes_per_row = bytes_per_row_for(width);

  this->opaque = true;
  for (int i = 0; i < width * height; i++) {
    if (pixels[i] == transparent) {
      this->opaque = false;
      break;
    }
  }

  for (int phase = 0; phase < SPRITE_PHASES; phase++) {
    for (int plane = 0; plane < 4; plane++) {
      if (!build_plane(&this->plane[phase][plane], pixels, width, height, phase,
                       plane, this->opaque, transparent,
                       this->bytes_per_row)) {
        return false;
      }
    }
  }
  return true;
}

// Reads one declared sheet and appends its cels to the set.
static bool load_sheet(sprite_set *this, const scene_sheet *sheet, char *error,
                       size_t error_size) {
  const char *path = sheet->path;

  FILE *file = fopen(path, "rb");
  if (!file) {
    snprintf(error, error_size, "cannot open sprite sheet %s", path);
    return false;
  }

  uint8_t header[SHEET_HEADER_SIZE];
  if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
      memcmp(header, SHEET_MAGIC, 4) != 0) {
    fclose(file);
    snprintf(error, error_size, "%s is not a sprite sheet", path);
    return false;
  }

  int width = (int)(header[4] | ((uint16_t)header[5] << 8));
  int height = (int)(header[6] | ((uint16_t)header[7] << 8));
  int count = (int)(header[8] | ((uint16_t)header[9] << 8));

  if (width < 4 || width > SCREEN_WIDTH || height < 1 ||
      height > SCREEN_HEIGHT) {
    fclose(file);
    snprintf(error, error_size, "%s has %dx%d cels, which do not fit", path,
             width, height);
    return false;
  }
  // The scene's count is what the behaviour was compiled against, so a file
  // that disagrees is a build problem, not something to paper over.
  if (count != sheet->cel_count) {
    fclose(file);
    snprintf(error, error_size, "%s holds %d cels, but the scene says %d", path,
             count, sheet->cel_count);
    return false;
  }
  if (this->count + count > SCENE_MAX_CELS) {
    fclose(file);
    snprintf(error, error_size, "%s would take the scene past %d cels", path,
             SCENE_MAX_CELS);
    return false;
  }

  uint8_t palette[SPRITE_PALETTE_BYTES];
  uint8_t transparent[SCENE_MAX_CELS];
  if (fread(palette, 1, SPRITE_PALETTE_BYTES, file) != SPRITE_PALETTE_BYTES ||
      fread(transparent, 1, (size_t)count, file) != (size_t)count) {
    fclose(file);
    snprintf(error, error_size, "%s ends inside its header", path);
    return false;
  }

  // Mode X shows one palette at a time, so every sheet in a scene has to be
  // indexed against the same one. Comparing is the only honest response.
  if (this->palette_loaded) {
    if (memcmp(this->palette, palette, SPRITE_PALETTE_BYTES) != 0) {
      fclose(file);
      snprintf(error, error_size,
               "%s uses a different palette from the sheets before it; a scene "
               "can only show one",
               path);
      return false;
    }
  } else {
    memcpy(this->palette, palette, SPRITE_PALETTE_BYTES);
    this->palette_loaded = true;
  }

  size_t cel_bytes = (size_t)width * (size_t)height;
  uint8_t *pixels = malloc(cel_bytes);
  if (!pixels) {
    fclose(file);
    snprintf(error, error_size, "out of memory for %s", path);
    return false;
  }

  for (int cel = 0; cel < count; cel++) {
    if (fread(pixels, 1, cel_bytes, file) != cel_bytes) {
      free(pixels);
      fclose(file);
      snprintf(error, error_size, "%s ends after %d of %d cels", path, cel,
               count);
      return false;
    }
    if (!sprite_cel_build(&this->cel[this->count + cel], pixels, width, height,
                          transparent[cel])) {
      free(pixels);
      fclose(file);
      snprintf(error, error_size, "%s: cel %d could not be built", path, cel);
      return false;
    }
  }

  free(pixels);
  fclose(file);
  this->count += count;
  return true;
}

bool sprite_set_load(sprite_set *this, const scene *scene, char *error,
                     size_t error_size) {
  memset(this, 0, sizeof(*this));

  for (int i = 0; i < scene->sheet_count; i++) {
    if (!load_sheet(this, &scene->sheet[i], error, error_size)) {
      return false;
    }
  }
  return this->count > 0;
}
