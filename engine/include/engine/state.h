#ifndef ENGINE_STATE_H_INCLUDED
#define ENGINE_STATE_H_INCLUDED

#include <stdint.h>

#include "engine/scene.h"
#include "engine/slots.h"

#define EMPTY_TILE (-1)

///
/// @brief The whole mutable world, as a flat table of slots.
///
/// Fixed size and pointer free. This is the layout the behaviour program
/// compiles against and the renderer reads, so there is exactly one
/// representation of the state in the engine.
///
typedef struct {
  int32_t slot[SLOT_COUNT];
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
uint32_t state_random(void);

///
/// @brief FNV-1a over the state, for replay and determinism checks.
///
uint32_t state_checksum(const world_state *this);

#endif
