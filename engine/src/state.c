#include "engine/state.h"

#include <string.h>

void state_init(world_state *this, const scene *scene) {
  memset(this, 0, sizeof(*this));

  int base = scene->entity_count * ENTITY_STRIDE;
  for (int i = 0; i < scene->global_count; i++) {
    this->slot[base + i] = scene->global[i].initial;
  }
}

uint32_t state_checksum(const world_state *this, int slot_count) {
  const uint8_t *bytes = (const uint8_t *)this->slot;
  uint32_t hash = 2166136261u;
  size_t size = (size_t)slot_count * sizeof(this->slot[0]);
  for (size_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= 16777619u;
  }
  return hash;
}
