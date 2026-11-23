#include "engine/state.h"

#include <string.h>

// The generator state is not part of the world state: only the values it has
// produced matter, and those live in the slots, so checksums stay stable.
static uint32_t random_state = 1;

uint32_t state_random(void) {
  uint32_t value = random_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  random_state = value;
  return value;
}

void state_init(world_state *this, const scene *scene) {
  memset(this, 0, sizeof(*this));

  this->slot[SLOT_CURSOR_ROW] = scene->rows / 2;
  this->slot[SLOT_CURSOR_COLUMN] = scene->columns / 2;
  this->slot[SLOT_PICKED_ROW] = -1;
  this->slot[SLOT_PICKED_COLUMN] = -1;

  random_state = scene->seed ? scene->seed : 1u;

  for (int row = 0; row < scene->rows; row++) {
    for (int column = 0; column < scene->columns; column++) {
      this->slot[SLOT_TILE(row, column)] =
          (int32_t)(state_random() % (uint32_t)scene->sprite_count);
    }
  }
}

uint32_t state_checksum(const world_state *this) {
  const uint8_t *bytes = (const uint8_t *)this->slot;
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < sizeof(this->slot); i++) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}
