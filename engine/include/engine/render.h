#ifndef ENGINE_RENDER_H_INCLUDED
#define ENGINE_RENDER_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/scene.h"
#include "engine/state.h"

///
/// @brief Per-stage cost of the last frame, for the frame budget.
///
typedef struct {
  uint32_t clear_us;
  uint32_t tiles_us;
  uint32_t border_us;
} render_stats;

void render_last_stats(render_stats *stats);

///
/// @brief Prepares the tile sheet and palette for @p scene.
///
/// @return false when the scene does not describe the sprite sheet the engine
/// was built with.
///
bool render_init(const scene *scene, char *error, size_t error_size);

///
/// @brief Draws one frame of @p state into the page at @p page_base.
///
void render_frame(const scene *scene, const world_state *state,
                  size_t page_base);

///
/// @brief Offset, in per-plane bytes, of the second display page.
///
size_t render_page_size(void);

#endif
