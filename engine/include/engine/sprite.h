#ifndef ENGINE_SPRITE_H_INCLUDED
#define ENGINE_SPRITE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/scene.h"

// A sprite is stored pre-shifted into the four horizontal alignments, so
// drawing never has to shift bits at run time: the phase is simply the low two
// bits of the destination x coordinate.
//
// In Mode X one byte of a plane holds exactly one pixel per plane (plane P of
// byte k is the screen pixel 4k + P). That makes a byte-granular presence mask
// exactly per-pixel accurate, and it means writing a byte in plane P can never
// disturb the three pixels that share it in the other planes.

#define SPRITE_PHASES 4
#define SPRITE_MAX_COUNT 64
#define SPRITE_PALETTE_BYTES (256 * 3)

typedef struct {
  uint8_t *data;    // bytes_per_row * height, colour indices
  uint8_t *present; // bytes_per_row * height, 1 where the cel has a pixel;
                    // NULL when the whole cel is opaque
  // Byte extent of this plane. For an opaque cel every pixel is present, so the
  // extent is the same on every row and the renderer can hoist it out of the
  // row loop; a cel with holes needs it per row instead.
  uint8_t first;
  uint8_t last;
  uint8_t *row_first; // height entries; NULL when opaque
  uint8_t *row_last;  // height entries; NULL when opaque
} sprite_plane;

typedef struct {
  int width;
  int height;
  int bytes_per_row;
  bool opaque;
  sprite_plane plane[SPRITE_PHASES][4];
} sprite_cel;

typedef struct {
  int count;
  bool palette_loaded;
  // As set_palette expects it: 256 RGB triples, eight bits each.
  uint8_t palette[SPRITE_PALETTE_BYTES];
  // Every sheet the scene names, appended in declaration order. A sprite handle
  // is an index into this, which is why the handles are stable and flat.
  sprite_cel cel[SCENE_MAX_CELS];
} sprite_set;

///
/// @brief Loads every sheet @p scene declares, in order.
///
/// Art is data on disk like the scene and the behaviour, so replacing a sheet
/// needs no rebuild. The file format is described by tools/png_to_spr.py.
///
/// @return false and fills @p error when a file is missing, is not a sprite
/// sheet, is truncated, or does not match what the scene declared.
///
bool sprite_set_load(sprite_set *this, const scene *scene, char *error,
                     size_t error_size);

///
/// @brief Builds one cel from chunky indexed pixels.
///
/// Pixels equal to @p transparent are not written when the cel is drawn. A cel
/// with no transparent pixels is marked opaque and takes the memcpy path.
///
bool sprite_cel_build(sprite_cel *this, const uint8_t *pixels, int width,
                      int height, uint8_t transparent);

#endif
