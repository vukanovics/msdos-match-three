#ifndef ENGINE_SPRITE_H_INCLUDED
#define ENGINE_SPRITE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
  int width;
  int height;
  const uint8_t *palette; // 256 entries of three bytes
  sprite_cel cel[SPRITE_MAX_COUNT];
} sprite_sheet;

///
/// @brief A sheet the engine can build, named by the scene's 'sheet' key.
///
/// The engine knows nothing about any particular sheet: the game supplies its
/// providers to engine_main. Replacing this with a loader for a sprites file on
/// disk is a change of provider, not a change of engine.
///
typedef struct {
  const char *name;
  bool (*build)(sprite_sheet *sheet, char *error, size_t error_size);
} sheet_source;

///
/// @brief Builds the cel named by @p name from @p sources.
///
/// @return false and fills @p error when no provider claims the name or the
/// provider rejects it.
///
bool sprite_sheet_load(sprite_sheet *this, const sheet_source *const *sources,
                       const char *name, char *error, size_t error_size);

///
/// @brief Builds one cel from chunky indexed pixels.
///
/// Pixels equal to @p transparent are not written when the cel is drawn. A cel
/// with no transparent pixels is marked opaque and takes the memcpy path.
///
bool sprite_cel_build(sprite_cel *this, const uint8_t *pixels, int width,
                      int height, uint8_t transparent);

#endif
