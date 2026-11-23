#include "engine/sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vga/vga.h"

// File layout, matching tools/build_gems.py.
#define SHEET_MAGIC "SPR1"
#define SHEET_HEADER_SIZE 10

// Room for the widest alignment: pixel width - 1 shifted by up to 3, then
// rounded up to a whole byte, plus the byte that the shift spills into.
static int bytes_per_row_for(int width) { return ((width + 2) >> 2) + 1; }

void sprite_cel_free(sprite_cel *this) {
  for (int plane = 0; plane < 4; plane++) {
    free(this->plane[plane].data);
    free(this->plane[plane].present);
    free(this->plane[plane].row_masked);
    free(this->plane[plane].row_left);
    free(this->plane[plane].row_right);
  }
  memset(this, 0, sizeof(*this));
}

// Grows @p this to hold @p bytes of plane, reusing what is already there. A cel
// is rebuilt every time the animation advances, and allocating in that path cost
// more than the decode did.
static bool ensure_capacity(sprite_plane *this, size_t bytes, int height,
                            bool opaque) {
  if (this->data_capacity < bytes) {
    free(this->data);
    this->data = malloc(bytes);
    this->data_capacity = this->data ? bytes : 0;
  }
  if (opaque) {
    free(this->present);
    free(this->row_masked);
    free(this->row_left);
    free(this->row_right);
    this->present = NULL;
    this->row_masked = NULL;
    this->row_left = NULL;
    this->row_right = NULL;
    this->mask_capacity = 0;
    this->rows_capacity = 0;
    return this->data != NULL;
  }
  if (this->mask_capacity < bytes) {
    free(this->present);
    this->present = malloc(bytes);
    this->mask_capacity = this->present ? bytes : 0;
  }
  if (this->rows_capacity < height) {
    free(this->row_masked);
    free(this->row_left);
    free(this->row_right);
    this->row_masked = malloc((size_t)height);
    this->row_left = malloc((size_t)height);
    this->row_right = malloc((size_t)height);
    this->rows_capacity =
        (this->row_masked && this->row_left && this->row_right) ? height : 0;
  }
  if (!this->data || !this->present || !this->row_masked || !this->row_left ||
      !this->row_right) {
    return false;
  }
  memset(this->present, 0, bytes);
  memset(this->row_masked, 0, (size_t)height);
  return true;
}

// Measures each row of @p target: where its pixels start and end, whether it
// has a hole between them, and which run of rows fills the whole extent.
static void measure_rows(sprite_plane *target, int height, int stride) {
  uint8_t row_low[SCREEN_HEIGHT];
  uint8_t row_high[SCREEN_HEIGHT];

  int plane_low = 0xff;
  int plane_high = 0;

  for (int row = 0; row < height; row++) {
    const uint8_t *mask = target->present + (size_t)row * (size_t)stride;
    int low = -1;
    int high = -1;
    int count = 0;
    for (int byte = 0; byte < stride; byte++) {
      if (mask[byte]) {
        if (low < 0) {
          low = byte;
        }
        high = byte;
        count++;
      }
    }
    if (low < 0) {
      row_low[row] = 0xff; // nothing in this row
      row_high[row] = 0;
      target->row_masked[row] = 1;
      continue;
    }
    row_low[row] = (uint8_t)low;
    row_high[row] = (uint8_t)high;
    target->row_masked[row] = (count != high - low + 1) ? 1 : 0;
    if (low < plane_low) {
      plane_low = low;
    }
    if (high > plane_high) {
      plane_high = high;
    }
  }

  target->first = (uint8_t)plane_low;
  target->last = (uint8_t)plane_high;
  target->band_first = 1;
  target->band_last = 0;

  if (plane_low > plane_high) {
    for (int row = 0; row < height; row++) {
      target->row_left[row] = 0;
      target->row_right[row] = 0;
    }
    return;
  }

  // How much of the plane extent each row leaves out at either end, which for a
  // rounded shape is one byte.
  for (int row = 0; row < height; row++) {
    if (row_low[row] == 0xff) {
      target->row_left[row] = 0;
      target->row_right[row] = 0;
      continue;
    }
    target->row_left[row] = (uint8_t)(row_low[row] - plane_low);
    target->row_right[row] = (uint8_t)(plane_high - row_high[row]);
  }

  // The longest run of rows that fill the whole extent: those need no per-row
  // bookkeeping at all when drawing.
  int best_start = 0;
  int best_length = 0;
  int run = 0;
  for (int row = 0; row < height; row++) {
    if (!target->row_masked[row] && target->row_left[row] == 0 &&
        target->row_right[row] == 0) {
      run++;
      if (run > best_length) {
        best_length = run;
        best_start = row - run + 1;
      }
    } else {
      run = 0;
    }
  }
  if (best_length > 0) {
    target->band_first = (uint8_t)best_start;
    target->band_last = (uint8_t)(best_start + best_length - 1);
  }
}

bool sprite_cel_build(sprite_cel *this, const uint8_t *pixels, int width,
                      int height, uint8_t transparent, int phase) {
  if (phase < 0 || phase >= SPRITE_PHASES) {
    return false;
  }
  this->width = width;
  this->height = height;
  this->bytes_per_row = bytes_per_row_for(width);

  int stride = this->bytes_per_row;
  size_t plane_bytes = (size_t)stride * (size_t)height;
  size_t pixel_count = (size_t)width * (size_t)height;

  this->opaque = true;
  for (size_t i = 0; i < pixel_count; i++) {
    if (pixels[i] == transparent) {
      this->opaque = false;
      break;
    }
  }

  for (int plane = 0; plane < 4; plane++) {
    if (!ensure_capacity(&this->plane[plane], plane_bytes, height,
                         this->opaque)) {
      sprite_cel_free(this);
      return false;
    }
  }

  // One pass over the cel, dropping each pixel into the plane its column lands
  // in. Walking the planes separately would test every pixel four times, and a
  // cel is rebuilt every time the animation advances. Within a group of four
  // columns the plane rotates the same way every time, so the four targets are
  // hoisted out of the loop.
  sprite_plane *group[4];
  size_t in_group[4];
  for (int k = 0; k < 4; k++) {
    group[k] = &this->plane[(phase + k) & 3];
    in_group[k] = (size_t)((phase + k) >> 2);
  }

  for (int row = 0; row < height; row++) {
    const uint8_t *source_row = pixels + (size_t)row * (size_t)width;
    size_t base = (size_t)row * (size_t)stride;
    int column = 0;
    for (; column + 3 < width; column += 4) {
      size_t byte = base + ((size_t)column >> 2);
      for (int k = 0; k < 4; k++) {
        uint8_t colour = source_row[column + k];
        if (!this->opaque && colour == transparent) {
          continue;
        }
        sprite_plane *target = group[k];
        target->data[byte + in_group[k]] = colour;
        if (target->present) {
          target->present[byte + in_group[k]] = 1;
        }
      }
    }
    for (; column < width; column++) {
      uint8_t colour = source_row[column];
      if (!this->opaque && colour == transparent) {
        continue;
      }
      int shifted = phase + column;
      sprite_plane *target = &this->plane[shifted & 3];
      size_t at = base + (size_t)(shifted >> 2);
      target->data[at] = colour;
      if (target->present) {
        target->present[at] = 1;
      }
    }
  }

  if (this->opaque) {
    // Every pixel is written, so each plane's bytes are one contiguous run on
    // every row, and the run follows from the phase: plane P owns the columns
    // congruent to P - phase, four apart.
    for (int plane = 0; plane < 4; plane++) {
      sprite_plane *target = &this->plane[plane];
      int first_column = (plane - phase) & 3;
      target->band_first = 0;
      target->band_last = (uint8_t)(height - 1);
      if (first_column >= width) {
        target->first = 1;
        target->last = 0; // this plane holds nothing
        continue;
      }
      int last_column = first_column + ((width - 1 - first_column) / 4) * 4;
      target->first = (uint8_t)((phase + first_column) >> 2);
      target->last = (uint8_t)((phase + last_column) >> 2);
    }
    return true;
  }

  for (int plane = 0; plane < 4; plane++) {
    measure_rows(&this->plane[plane], height, stride);
  }
  return true;
}

const sprite_cel *sprite_cel_at(sprite_set *this, int cel, int phase) {
  if (cel < 0 || cel >= this->count) {
    return NULL;
  }

  // A resident cel is decoded once, at the alignment everything at rest uses,
  // and kept. Animation then costs nothing to draw: the cel is already split
  // into planes, and the blit is the same copy whatever cel it is.
  if (phase == 0 && this->resident_cel && this->resident_cel[cel]) {
    sprite_cel *held = &this->resident[cel];
    if (held->width == 0) {
      const sprite_image *image = &this->image[cel];
      if (!sprite_cel_build(held, image->pixels, image->width, image->height,
                            image->transparent, 0)) {
        return NULL;
      }
      this->decodes++;
    }
    return held;
  }

  this->clock++;
  int victim = 0;
  for (int i = 0; i < SPRITE_CACHE_ENTRIES; i++) {
    if (this->cache_cel[i] == cel && this->cache_phase[i] == phase) {
      this->cache_age[i] = this->clock;
      return &this->cache[i];
    }
    if (this->cache_cel[i] < 0) {
      victim = i;
      continue;
    }
    if (this->cache_age[i] < this->cache_age[victim]) {
      victim = i;
    }
  }

  const sprite_image *image = &this->image[cel];
  if (!sprite_cel_build(&this->cache[victim], image->pixels, image->width,
                        image->height, image->transparent, phase)) {
    this->cache_cel[victim] = -1;
    return NULL;
  }
  this->decodes++;
  this->cache_cel[victim] = cel;
  this->cache_phase[victim] = phase;
  this->cache_age[victim] = this->clock;
  return &this->cache[victim];
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
  for (int cel = 0; cel < count; cel++) {
    uint8_t *pixels = malloc(cel_bytes);
    if (!pixels) {
      fclose(file);
      snprintf(error, error_size, "out of memory for %s", path);
      return false;
    }
    if (fread(pixels, 1, cel_bytes, file) != cel_bytes) {
      free(pixels);
      fclose(file);
      snprintf(error, error_size, "%s ends after %d of %d cels", path, cel,
               count);
      return false;
    }
    sprite_image *image = &this->image[this->count + cel];
    image->width = width;
    image->height = height;
    image->transparent = transparent[cel];
    image->pixels = pixels;
  }

  fclose(file);
  this->count += count;
  return true;
}

bool sprite_set_load(sprite_set *this, const scene *scene, char *error,
                     size_t error_size) {
  memset(this, 0, sizeof(*this));
  for (int i = 0; i < SPRITE_CACHE_ENTRIES; i++) {
    this->cache_cel[i] = -1;
  }

  int sheet_base[SCENE_MAX_SHEETS];
  for (int i = 0; i < scene->sheet_count; i++) {
    sheet_base[i] = this->count;
    if (!load_sheet(this, &scene->sheet[i], error, error_size)) {
      return false;
    }
  }
  if (this->count == 0) {
    snprintf(error, error_size, "the scene loaded no cels");
    return false;
  }

  this->anim_count = scene->sprite_count;
  for (int i = 0; i < scene->sprite_count; i++) {
    const scene_sprite *sprite = &scene->sprite[i];
    this->anim[i].first_cel = sheet_base[sprite->sheet] + sprite->cel;
    this->anim[i].frames = sprite->frames;
    this->anim[i].loop = sprite->loop;
  }

  // Decode what the scene asked to keep, now rather than on first use: the cels
  // an animation run needs are wanted in one go the first time it plays, and
  // building them here spends that cost once, before the first frame, instead of
  // spread over the first loop of the animation.
  int wanted = 0;
  for (int i = 0; i < scene->sprite_count; i++) {
    if (scene->sprite[i].resident) {
      wanted++;
    }
  }
  if (wanted == 0) {
    return true;
  }

  this->resident = calloc((size_t)this->count, sizeof(sprite_cel));
  this->resident_cel = calloc((size_t)this->count, 1);
  if (!this->resident || !this->resident_cel) {
    // Not fatal: without the table the cache serves every cel, just more slowly.
    free(this->resident);
    free(this->resident_cel);
    this->resident = NULL;
    this->resident_cel = NULL;
    return true;
  }

  for (int i = 0; i < scene->sprite_count; i++) {
    if (!scene->sprite[i].resident) {
      continue;
    }
    int first = this->anim[i].first_cel;
    for (int frame = 0; frame < this->anim[i].frames; frame++) {
      this->resident_cel[first + frame] = 1;
      if (!sprite_cel_at(this, first + frame, 0)) {
        snprintf(error, error_size, "out of memory decoding a resident sprite");
        return false;
      }
    }
  }
  // Decoding at load is not a frame; do not let it show up as one.
  this->decodes = 0;
  return true;
}
