#include "engine/anim.h"

#include "engine/slots.h"

bool anim_advance(const scene *scene, world_state *state, vm *behaviour) {
  for (int entity = 0; entity < scene->entity_count; entity++) {
    int32_t *slots = &state->slot[entity * ENTITY_STRIDE];

    // An entity the game is not processing keeps whatever cel it is showing. It
    // also does not report an end, because nothing happened to it.
    if (!(slots[ENTITY_FLAGS] & ENTITY_PROCESS)) {
      continue;
    }

    int handle = slots[ENTITY_ANIMATION];
    if (handle < 0 || handle >= scene->sprite_count) {
      continue;
    }

    // A still has nothing to play, so it neither moves nor ends. That is what
    // keeps a cursor or a marker out of the end report entirely.
    const scene_sprite *anim = &scene->sprite[handle];
    if (anim->frames <= 1 || anim->ticks < 1) {
      continue;
    }

    // Each animation holds its cels for its own number of ticks, so the count
    // down is per entity rather than a common step everyone shares.
    if (slots[ENTITY_PHASE] + 1 < anim->ticks) {
      slots[ENTITY_PHASE]++;
      continue;
    }
    slots[ENTITY_PHASE] = 0;

    int32_t cel = slots[ENTITY_FRAME];
    if (cel < 0) {
      cel = 0;
    }

    if (anim->loop) {
      slots[ENTITY_FRAME] = (cel + 1) % anim->frames;
      continue;
    }
    if (cel + 1 >= anim->frames) {
      continue; // on the last cel already; that was reported once
    }

    slots[ENTITY_FRAME] = cel + 1;
    if (cel + 1 == anim->frames - 1) {
      if (!vm_animation_end(behaviour, entity)) {
        return false;
      }
    }
  }
  return true;
}
