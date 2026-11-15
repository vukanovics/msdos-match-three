#ifndef GAME_H_INCLUDED
#define GAME_H_INCLUDED

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
/// @brief Draws the whole board and the current selection.
///
void game_render(game *this);

///
/// @brief Moves the selection cursor by one tile, clamped to the board.
///
void game_move_selection(game *this, int dx, int dy);

///
/// @brief Handles the enter key.
///
/// First press picks the tile under the cursor. The second press swaps it with
/// the tile under the cursor, clears the pick, and resolves matches.
///
void game_select(game *this);

#endif
