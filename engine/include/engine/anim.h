#ifndef ENGINE_ANIM_H_INCLUDED
#define ENGINE_ANIM_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "engine/scene.h"
#include "engine/state.h"
#include "engine/vm.h"

///
/// @brief One frame of the engine's animation clock.
///
/// Animation is world state: an entity names an animation and the cel it is
/// showing, and the engine moves the cel along. Each animation holds its cels
/// for its own tick count, so every entity carries a small countdown.
///
/// A run that loops wraps round; a run that does not loop stops on its last cel,
/// and arriving there is the event - the behaviour's on-animation-end entry is
/// called with the entity, so "play this once, then go back to idle" needs no
/// bookkeeping in the program.
///
/// An entity whose ENTITY_PROCESS bit is clear is left exactly as it is: no cel
/// change and no end report.
///
/// Only the scene is needed, not the loaded cels: which cels a handle covers is
/// a question for the renderer.
///
/// @return false if the behaviour faulted; @p behaviour then carries the error.
///
bool anim_advance(const scene *scene, world_state *state, vm *behaviour);

#endif
