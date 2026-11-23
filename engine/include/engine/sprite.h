#ifndef ENGINE_SPRITE_H_INCLUDED
#define ENGINE_SPRITE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/scene.h"

// A cel is decoded one horizontal alignment at a time. A 24x24 cel is 576 bytes
// of source but four times that once split into planes, and a scene can name
// hundreds of cels - a gem sheet of 120 frames is 420 KB of source and would be
// 4 MB decoded. So the source stays chunky, and the alignments the renderer
// actually asks for are decoded into a small cache.
//
// In Mode X one byte of a plane holds exactly one pixel per plane (plane P of
// byte k is the screen pixel 4k + P). That makes a byte-granular presence mask
// exactly per-pixel accurate, and it means writing a byte in plane P can never
// disturb the three pixels that share it in the other planes.

#define SPRITE_PHASES 4
#define SPRITE_PALETTE_BYTES (256 * 3)
#define SPRITE_CACHE_ENTRIES 16

typedef struct {
  uint8_t *data;    // bytes_per_row * height, colour indices
  uint8_t *present; // bytes_per_row * height, 1 where the cel has a pixel;
                    // NULL when the whole cel is opaque
  // Byte extent of this plane. For an opaque cel every pixel is present, so the
  // extent is the same on every row and the renderer can hoist it out of the
  // row loop; a cel with holes needs it per row instead.
  uint8_t first;
  uint8_t last;
  uint8_t *row_masked; // height entries; 1 where a row has a hole inside its
                       // run, so it has to be tested byte by byte

  // The run of rows that use the whole plane extent and have no holes. For a
  // rounded shape that is everything except a band at the top and bottom, and
  // inside it the renderer needs no per-row bookkeeping at all.
  uint8_t band_first;
  uint8_t band_last;

  // The buffers above are kept between decodes of the same cache slot. A cel is
  // rebuilt every time the animation advances, and allocating there cost more
  // than the decode itself.
  size_t data_capacity;
  size_t mask_capacity;
  int rows_capacity;
  // Height entries, NULL when opaque. first/last above are the extent over the
  // whole plane; these say how much of that a given row leaves out at each end,
  // so the row loop still copies one run and only touches two bytes per row.
  uint8_t *row_left;
  uint8_t *row_right;
} sprite_plane;

typedef struct {
  int width;
  int height;
  int bytes_per_row;
  bool opaque;
  sprite_plane plane[4]; // the planes of one horizontal alignment
} sprite_cel;

// A cel as it comes off disk: chunky palette indices, not yet split up.
typedef struct {
  int width;
  int height;
  uint8_t transparent;
  uint8_t *pixels; // width * height, owned
} sprite_image;

///
/// @brief A named sprite: the first of its cels, and how many it runs through.
///
/// One frame is a still sprite, so a plain sprite and an animation are the same
/// thing as far as the engine is concerned.
///
typedef struct {
  int first_cel;
  int frames;
} sprite_anim;

typedef struct {
  int count; // cels loaded, across every sheet
  bool palette_loaded;
  // As set_palette expects it: 256 RGB triples, eight bits each.
  uint8_t palette[SPRITE_PALETTE_BYTES];

  sprite_image image[SCENE_MAX_CELS];

  int anim_count;
  sprite_anim anim[SCENE_MAX_SPRITES];

  // Decoded (cel, phase) pairs, least recently used first out.
  int cache_cel[SPRITE_CACHE_ENTRIES];
  int cache_phase[SPRITE_CACHE_ENTRIES];
  uint32_t cache_age[SPRITE_CACHE_ENTRIES];
  uint32_t clock;
  uint32_t decodes; // cels built since the last time it was read
  sprite_cel cache[SPRITE_CACHE_ENTRIES];
} sprite_set;

///
/// @brief Loads every sheet @p scene declares, in order.
///
/// Art is data on disk like the scene and the behaviour, so replacing a sheet
/// needs no rebuild. The file format is described by tools/build_gems.py.
///
/// @return false and fills @p error when a file is missing, is not a sprite
/// sheet, is truncated, or does not match what the scene declared.
///
bool sprite_set_load(sprite_set *this, const scene *scene, char *error,
                     size_t error_size);

///
/// @brief The cel @p cel decoded for the alignment @p phase, from the cache
/// when it is already there.
///
/// The returned pointer stays valid until enough other cels are asked for that
/// it is evicted, which is why the renderer decodes and draws one entity at a
/// time.
///
const sprite_cel *sprite_cel_at(sprite_set *this, int cel, int phase);

///
/// @brief Decodes one alignment of @p pixels into @p this.
///
bool sprite_cel_build(sprite_cel *this, const uint8_t *pixels, int width,
                      int height, uint8_t transparent, int phase);

///
/// @brief Releases everything @p this owns.
///
void sprite_cel_free(sprite_cel *this);

#endif
