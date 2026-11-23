#ifndef ENGINE_SLOTS_H_INCLUDED
#define ENGINE_SLOTS_H_INCLUDED

// The engine's one built-in idea about the world: an ordered pool of entities,
// each of which is a sprite at a position. The pool occupies the first
// entity_count * ENTITY_STRIDE slots, so entity e owns the slots starting at
// e * ENTITY_STRIDE. Anything else the game needs is a named slot declared by
// the scene, which follow the pool.
//
// Nothing here knows what a tile, a cursor or a board is.

#define ENTITY_STRIDE 5

#define ENTITY_SPRITE 0
#define ENTITY_X 1
#define ENTITY_Y 2
#define ENTITY_FLAGS 3

// The tick at which the entity's current animation began. The renderer works
// out which frame that means from the frame counter, so an animation is never
// part of the world state - only the moment it started is.
#define ENTITY_ANIM_START 4

#define ENTITY_VISIBLE 0x01

// Positions are Q16.16: pixel = value / 65536.
#define ENTITY_FIXED_SHIFT 16

#define ENTITY_SLOT(entity, field) ((entity) * ENTITY_STRIDE + (field))

#endif
