#ifndef ENGINE_STATE_H_INCLUDED
#define ENGINE_STATE_H_INCLUDED

#include <stdint.h>

#include "engine/scene.h"

#define EMPTY_TILE (-1)

///
/// @brief The whole mutable world.
///
/// Fixed size and pointer free: this is the layout the functional update
/// (state, input) -> state compiles against, and it can be copied or checksummed
/// cheaply. Entity pools for things created at runtime belong here too, with
/// their capacities known at build time.
///
typedef struct {
  int8_t tiles[SCENE_MAX_ROWS][SCENE_MAX_COLUMNS];
  int8_t cursor_row;
  int8_t cursor_column;
  int8_t picked_row;
  int8_t picked_column;
  uint32_t random;
} world_state;

///
/// @brief Builds the initial state for @p scene.
///
/// Seeded from the scene, so the same scene always starts with the same board.
///
void state_init(world_state *this, const scene *scene);

///
/// @brief xorshift32; deterministic across builds and machines.
///
uint32_t state_random(world_state *this);

///
/// @brief FNV-1a over the state, for replay and determinism checks.
///
uint32_t state_checksum(const world_state *this);

#endif
