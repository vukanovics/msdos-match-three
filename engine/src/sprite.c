#include "engine/sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool sprite_sheet_load(sprite_sheet *this, const sheet_source *const *sources,
                       const char *name, char *error, size_t error_size) {
  memset(this, 0, sizeof(*this));

  if (!sources) {
    snprintf(error, error_size, "no sprite sheets are available");
    return false;
  }
  for (int i = 0; sources[i]; i++) {
    if (!strcmp(sources[i]->name, name)) {
      return sources[i]->build(this, error, error_size);
    }
  }

  snprintf(error, error_size, "no sprite sheet named '%s'", name);
  return false;
}
