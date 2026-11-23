#ifndef ENGINE_RENDER_H_INCLUDED
#define ENGINE_RENDER_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/scene.h"
#include "engine/sprite.h"
#include "engine/state.h"

///
/// @brief Per-stage cost of the last frame, for the frame budget.
///
typedef struct {
  uint32_t clear_us;
  uint32_t sprites_us;
} render_stats;

void render_last_stats(render_stats *stats);

///
/// @brief Prepares the palette and the draw state for @p scene.
///
/// The renderer draws whatever the entity pool says: it has no notion of a
/// board, a tile or a cursor, only of sprites at positions.
///
bool render_init(const scene *scene, const sprite_set *sprites, char *error,
                 size_t error_size);

///
/// @brief Draws one frame of @p state into the page at @p page_base.
///
void render_frame(const scene *scene, const sprite_set *sprites,
                  const world_state *state, size_t page_base);

///
/// @brief Offset, in per-plane bytes, of the second display page.
///
size_t render_page_size(void);

#endif
