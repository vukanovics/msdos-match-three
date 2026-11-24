#ifndef ENGINE_PARTICLES_H_INCLUDED
#define ENGINE_PARTICLES_H_INCLUDED

#include "engine/sprite.h"
#include "engine/state.h"

#define PARTICLE_CAPACITY 384
#define PARTICLE_SUBPIXEL_SHIFT 8

typedef struct {
  int32_t x, y;
  int16_t vx, vy;
  uint8_t size, colour, lifetime;
} particle;

typedef struct {
  const scene *description;
  const sprite_set *sprites;
  const world_state *world;
  uint32_t random_state;
  unsigned count;
  particle items[PARTICLE_CAPACITY];
} particle_system;

void particles_init(particle_system *system, const scene *description,
                    const sprite_set *sprites, const world_state *world);
void particles_advance(particle_system *system);
// VM callback. Reads the entity's current cel before the script removes it.
void particles_emit(void *context, int entity);

#endif
