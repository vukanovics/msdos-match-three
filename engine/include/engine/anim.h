#ifndef ENGINE_ANIM_H_INCLUDED
#define ENGINE_ANIM_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "engine/scene.h"
#include "engine/state.h"
#include "engine/vm.h"

///
/// @brief Steps every entity's cel, and reports the runs that end.
///
/// Animation is world state: an entity names an animation and the cel it is
/// showing, and the engine moves the cel along. A run that loops wraps round; a
/// run that does not loop stops on its last cel, and arriving there is the
/// event - the behaviour's on-animation-end entry is called with the entity, so
/// "play this once, then go back to idle" needs no bookkeeping in the program.
///
/// Every entity steps on the same tick, once per @c scene->animation_ticks
/// frames. An animation started between steps shows its first cel for the rest
/// of the quantum, which is at most a couple of frames.
///
/// Only the scene is needed, not the loaded cels: which cels a handle covers is
/// a question for the renderer.
///
/// @return false if the behaviour faulted; @p behaviour then carries the error.
///
bool anim_advance(const scene *scene, world_state *state, uint32_t frame,
                  vm *behaviour);

#endif
