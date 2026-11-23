#ifndef ENGINE_SCENE_H_INCLUDED
#define ENGINE_SCENE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SCENE_NAME_MAX 32
#define SCENE_MAX_COLUMNS 16
#define SCENE_MAX_ROWS 16

///
/// @brief Declarative description of a scene.
///
/// Resolved from human readable text when the scene loads, into a fixed layout
/// with every reference already resolved. Nothing here is allocated at runtime.
///
typedef struct {
  char name[SCENE_NAME_MAX];
  int columns;
  int rows;
  int tile_size;
  uint8_t background;
  uint8_t cursor;
  uint8_t picked;
  int sprite_count;
  int sprite_width;
  int sprite_height;
  uint32_t seed;
} scene;

///
/// @brief Parses @p path into @p this.
///
/// The file is line oriented: every non-blank, non-comment line is
/// "key value". Unknown keys and values out of range are errors, so a broken
/// scene fails at load time rather than mid-frame.
///
/// @return false and fills @p error with "line N: ..." when the file is invalid.
///
bool scene_load(scene *this, const char *path, char *error, size_t error_size);

///
/// @brief Describes the scene in one line, for diagnostics.
///
void scene_describe(const scene *this, char *buffer, size_t buffer_size);

#endif
