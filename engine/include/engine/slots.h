#ifndef ENGINE_SLOTS_H_INCLUDED
#define ENGINE_SLOTS_H_INCLUDED

// The engine's one built-in idea about the world: an ordered pool of entities,
// each of which is an animation at a position, playing a cel. The pool occupies
// the first entity_count * ENTITY_STRIDE slots, so entity e owns the slots
// starting at e * ENTITY_STRIDE. Anything else the game needs is a named slot
// declared by the scene, which follow the pool.
//
// Nothing here knows what a tile, a cursor or a board is.

#define ENTITY_STRIDE 5

// A handle into the scene's sprite table. A sprite of one cel is a still; a
// bigger one is an animation, and the engine steps it.
#define ENTITY_ANIMATION 0
#define ENTITY_X 1
#define ENTITY_Y 2
#define ENTITY_FLAGS 3

// Which cel of that animation is showing. The engine owns this: it steps it once
// every scene->animation_ticks and, for a run that does not loop, calls the
// behaviour's on-animation-end when the last cel is reached.
#define ENTITY_FRAME 4

#define ENTITY_VISIBLE 0x01

// Positions are Q16.16: pixel = value / 65536.
#define ENTITY_FIXED_SHIFT 16

#define ENTITY_SLOT(entity, field) ((entity) * ENTITY_STRIDE + (field))

#endif
