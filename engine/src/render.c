#include "engine/render.h"

#include <stdio.h>
#include <string.h>
#include <sys/nearptr.h>

#include "engine/clock.h"
#include "vga/vga.h"

// Per-plane bytes for one screen page.
#define PAGE_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT) / 4)

// The flip writes only the start address high byte, which is safe only while
// every page base is a multiple of 256 (see set_display_start). Fail the build
// rather than the display if the mode or page layout ever changes.
typedef char page_size_must_be_a_multiple_of_256[(PAGE_SIZE % 256 == 0) ? 1 : -1];

// Bytes per scanline in one plane.
#define ROW_BYTES (SCREEN_WIDTH / 4)

static render_stats last_stats;

void render_last_stats(render_stats *stats) { *stats = last_stats; }

static uint8_t *video_memory(void) {
  return (uint8_t *)(0xa0000 + __djgpp_conventional_base);
}

size_t render_page_size(void) { return PAGE_SIZE; }

bool render_init(const scene *scene, const sprite_set *source, char *error,
                 size_t error_size) {
  if (!source || source->count <= 0) {
    snprintf(error, error_size, "scene '%s' has no sprites", scene->name);
    return false;
  }

  set_palette(source->palette, 256);
  set_gc(&GC_BIT_MASK, 0xff);
  return true;
}

// Clearing touches every byte of one plane before moving to the next, so the
// sequencer mask is programmed four times instead of once per scanline.
static void clear_page(uint8_t *vram, size_t base, uint8_t colour) {
  for (int plane = 0; plane < 4; plane++) {
    set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));
    memset(vram + base, colour, PAGE_SIZE);
  }
}

// Mode X writes are gated by the sequencer map mask, and changing that mask is
// by far the most expensive part of a blit. Grouping the work by plane means
// four mask changes per sprite rather than one per scanline.
//
// The cel is pre-shifted, so the phase is just the low two bits of x; an
// opaque cel copies whole rows, and a cel with holes writes only the bytes that
// hold one of its pixels.
// One row of a cel that does not fill its plane's extent: it starts or ends
// short, or has a hole. Only the rounded corners of a shape land here.
static inline __attribute__((always_inline)) void
blit_narrow_row(uint8_t *vram, size_t page_base, int y,
                            const sprite_plane *source, int stride, int column,
                            int left, int right, int row) {
  int first = source->first + source->row_left[row];
  int last = source->last - source->row_right[row];
  if (first < left) {
    first = left;
  }
  if (last > right) {
    last = right;
  }
  if (first > last) {
    return;
  }

  const uint8_t *source_row = source->data + (size_t)row * (size_t)stride;
  uint8_t *destination = vram + page_base +
                         (size_t)(y + row) * ROW_BYTES + (size_t)column;
  if (source->row_masked[row]) {
    const uint8_t *mask = source->present + (size_t)row * (size_t)stride;
    for (int i = first; i <= last; i++) {
      if (mask[i]) {
        destination[i] = source_row[i];
      }
    }
    return;
  }
  for (int i = first; i <= last; i++) {
    destination[i] = source_row[i];
  }
}

// Mode X writes are gated by the sequencer map mask, and changing that mask is
// by far the most expensive part of a blit. Grouping the work by plane means
// four mask changes per sprite rather than one per scanline.
//
// The cel is decoded for one alignment, and the middle of it is a plain run of
// bytes per row, so that part copies with no per-row bookkeeping at all.
static void blit(const sprite_cel *cel, int x, int y, size_t page_base) {
  if (x <= -cel->width || x >= SCREEN_WIDTH) {
    return;
  }
  if (y <= -cel->height || y >= SCREEN_HEIGHT) {
    return;
  }

  // Floor division, so that x == 4 * column + (x & 3) holds for negative x too.
  int column = x >= 0 ? (x >> 2) : -((-x + 3) >> 2);

  int first_row = y < 0 ? -y : 0;
  int last_row = cel->height - 1;
  if (y + last_row >= SCREEN_HEIGHT) {
    last_row = SCREEN_HEIGHT - 1 - y;
  }
  int left = -column;
  if (left < 0) {
    left = 0;
  }
  int right = ROW_BYTES - 1 - column;

  uint8_t *vram = video_memory();
  int stride = cel->bytes_per_row;

  for (int plane = 0; plane < 4; plane++) {
    const sprite_plane *source = &cel->plane[plane];
    int first = source->first;
    int last = source->last;
    if (first > last) {
      continue;
    }
    if (first < left) {
      first = left;
    }
    if (last > right) {
      last = right;
    }
    if (first > last) {
      continue;
    }
    int count = last - first + 1;

    int band_first = source->band_first;
    int band_last = source->band_last;
    if (band_first < first_row) {
      band_first = first_row;
    }
    if (band_last > last_row) {
      band_last = last_row;
    }

    set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));

    for (int row = first_row; row < band_first; row++) {
      blit_narrow_row(vram, page_base, y, source, stride, column, left, right,
                      row);
    }

    if (band_first <= band_last) {
      const uint8_t *from = source->data + (size_t)band_first * (size_t)stride +
                            (size_t)first;
      uint8_t *to = vram + page_base +
                    (size_t)(y + band_first) * ROW_BYTES +
                    (size_t)(column + first);
      for (int row = band_first; row <= band_last; row++) {
        for (int i = 0; i < count; i++) {
          to[i] = from[i];
        }
        from += stride;
        to += ROW_BYTES;
      }
    }

    for (int row = band_last + 1; row <= last_row; row++) {
      blit_narrow_row(vram, page_base, y, source, stride, column, left, right,
                      row);
    }
  }
}

void render_frame(const scene *scene, sprite_set *source,
                  const world_state *state, size_t page_base) {
  uint8_t *vram = video_memory();
  uint32_t start = clock_us();

  set_draw_page(page_base);
  clear_page(vram, page_base, scene->background);
  uint32_t after_clear = clock_us();

  for (int entity = 0; entity < scene->entity_count; entity++) {
    const int32_t *slots = &state->slot[entity * ENTITY_STRIDE];
    if (!(slots[ENTITY_FLAGS] & ENTITY_VISIBLE)) {
      continue;
    }
    int handle = slots[ENTITY_ANIMATION];
    if (handle < 0 || handle >= source->anim_count) {
      continue;
    }

    // The world says which cel to show; the engine has already stepped it. A
    // cel number out of range would only come from a program writing the slot
    // itself, so clamp rather than let the blitter read past the run.
    const sprite_anim *anim = &source->anim[handle];
    int frame = slots[ENTITY_FRAME];
    if (frame < 0) {
      frame = 0;
    } else if (frame >= anim->frames) {
      frame = anim->frames - 1;
    }
    int cel = anim->first_cel + frame;

    int x = slots[ENTITY_X] >> ENTITY_FIXED_SHIFT;
    int y = slots[ENTITY_Y] >> ENTITY_FIXED_SHIFT;
    const sprite_cel *decoded = sprite_cel_at(source, cel, x & 3);
    if (!decoded) {
      continue;
    }
    blit(decoded, x, y, page_base);
  }
  uint32_t after_sprites = clock_us();

  last_stats.clear_us = after_clear - start;
  last_stats.sprites_us = after_sprites - after_clear;
  last_stats.decodes = source->decodes;
  source->decodes = 0;
}
