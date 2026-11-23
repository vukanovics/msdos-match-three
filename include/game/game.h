#ifndef GAME_H_INCLUDED
#define GAME_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

///
/// @brief Match-three game state.
///
typedef struct game_s game;

///
/// @brief Creates a game using the embedded match-three tile set.
///
/// Must be called after switch_to_mode_x() with near pointers enabled.
///
game *game_new(void);

void game_free(game *this);

///
/// @brief Draws the board, animations, and the current selection.
///
void game_render(game *this);

///
/// @brief Moves the selection cursor by one tile, clamped to the board.
///
/// Ignored while an animation is playing.
///
void game_move_selection(game *this, int dx, int dy);

///
/// @brief Handles the enter key.
///
/// First press picks the tile under the cursor. The second press starts an
/// animation that swaps it with the tile under the cursor.
///
/// Ignored while an animation is playing.
///
void game_select(game *this);

///
/// @brief Advances any running animation by @p elapsed_us microseconds.
///
/// @return true if the display needs to be redrawn.
///
bool game_update(game *this, uint32_t elapsed_us);

///
/// @brief Whether an animation is currently playing.
///
bool game_is_animating(game *this);

#endif
