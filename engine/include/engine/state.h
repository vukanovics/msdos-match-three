#ifndef ENGINE_STATE_H_INCLUDED
#define ENGINE_STATE_H_INCLUDED

#include <stdint.h>

#include "engine/scene.h"
#include "engine/slots.h"

///
/// @brief The whole mutable world, as a flat table of slots.
///
/// Fixed size and pointer free. The entity pool comes first, then the scene's
/// named globals, which is the layout the behaviour program compiles against
/// and the renderer reads, so there is exactly one representation of the state
/// in the engine.
///
typedef struct {
  int32_t slot[SCENE_MAX_SLOTS];
} world_state;

///
/// @brief Builds the initial state for @p scene.
///
/// Everything starts zeroed - which is what hides unused entities, since
/// ENTITY_VISIBLE is bit 0 of their flags - and the scene's declared global
/// values are copied in. Building the actual scene is the behaviour program's
/// job, not the engine's.
///
void state_init(world_state *this, const scene *scene);

///
/// @brief FNV-1a over the first @p slot_count slots, for replay and
/// determinism checks.
///
uint32_t state_checksum(const world_state *this, int slot_count);

#endif
