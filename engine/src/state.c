#include "engine/state.h"

#include <string.h>

void state_init(world_state *this, const scene *scene) {
  memset(this, 0, sizeof(*this));

  this->cursor_row = (int8_t)(scene->rows / 2);
  this->cursor_column = (int8_t)(scene->columns / 2);
  this->picked_row = -1;
  this->picked_column = -1;
  this->random = scene->seed ? scene->seed : 1u;

  for (int row = 0; row < scene->rows; row++) {
    for (int column = 0; column < scene->columns; column++) {
      this->tiles[row][column] =
          (int8_t)(state_random(this) % (uint32_t)scene->sprite_count);
    }
  }
}

uint32_t state_random(world_state *this) {
  uint32_t value = this->random;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  this->random = value;
  return value;
}

uint32_t state_checksum(const world_state *this) {
  const uint8_t *bytes = (const uint8_t *)this;
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < sizeof(*this); i++) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}
