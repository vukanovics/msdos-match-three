#include "engine/particles.h"

#define SUBPIXELS (1 << PARTICLE_SUBPIXEL_SHIFT)

static int sample(particle_system *system, particle_range range) {
  if (range.min == range.max) {
    return range.min;
  }
  uint32_t value = system->random_state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  system->random_state = value;
  return range.min + (int)(value % (unsigned)(range.max - range.min + 1));
}

static int colour_at(particle_system *system, const sprite_image *image,
                     int x, int y) {
  particle_range allowed = system->description->particles.colour;
  int colour = image->pixels[y * image->width + x];
  if (colour != image->transparent && colour >= allowed.min &&
      colour <= allowed.max) {
    return colour;
  }
  // Try other pixels when the origin is transparent or outside the palette range,
  // with a fixed work limit for very sparse sprites.
  particle_range pixels = {0, image->width * image->height - 1};
  for (int attempt = 0; attempt < 8; attempt++) {
    colour = image->pixels[sample(system, pixels)];
    if (colour != image->transparent && colour >= allowed.min &&
        colour <= allowed.max) {
      return colour;
    }
  }
  return -1;
}

void particles_init(particle_system *system, const scene *description,
                    const sprite_set *sprites, const world_state *world) {
  system->description = description;
  system->sprites = sprites;
  system->world = world;
  system->random_state = description->seed ^ 0x9e3779b9u;
  if (!system->random_state) {
    system->random_state = 1;
  }
  system->count = 0;
}

void particles_advance(particle_system *system) {
  int gravity = system->description->particles.gravity;
  unsigned live = 0;
  for (unsigned i = 0; i < system->count; i++) {
    particle item = system->items[i];
    if (--item.lifetime) {
      item.x += item.vx;
      item.y += item.vy;
      item.vy += gravity;
      system->items[live++] = item;
    }
  }
  system->count = live;
}

void particles_emit(void *context, int entity) {
  particle_system *system = context;
  const particle_config *config = &system->description->particles;
  if (!config->count.max || entity < 0 ||
      entity >= system->description->entity_count ||
      system->count == PARTICLE_CAPACITY) {
    return;
  }

  const int32_t *slots = &system->world->slot[entity * ENTITY_STRIDE];
  int handle = slots[ENTITY_ANIMATION];
  if (handle < 0 || handle >= system->sprites->anim_count) {
    return;
  }
  const sprite_anim *animation = &system->sprites->anim[handle];
  int frame = slots[ENTITY_FRAME];
  if (frame < 0) {
    frame = 0;
  } else if (frame >= animation->frames) {
    frame = animation->frames - 1;
  }
  const sprite_image *image = &system->sprites->image[animation->first_cel + frame];
  particle_range xs = {config->x_percent.min * (image->width - 1) / 100,
                       config->x_percent.max * (image->width - 1) / 100};
  particle_range ys = {config->y_percent.min * (image->height - 1) / 100,
                       config->y_percent.max * (image->height - 1) / 100};

  int count = sample(system, config->count);
  for (int i = 0; i < count && system->count < PARTICLE_CAPACITY; i++) {
    int x = sample(system, xs);
    int y = sample(system, ys);
    int colour = colour_at(system, image, x, y);
    if (colour < 0) {
      continue;
    }
    particle *item = &system->items[system->count++];
    item->x = (slots[ENTITY_X] >> (ENTITY_FIXED_SHIFT - PARTICLE_SUBPIXEL_SHIFT))
              + x * SUBPIXELS;
    item->y = (slots[ENTITY_Y] >> (ENTITY_FIXED_SHIFT - PARTICLE_SUBPIXEL_SHIFT))
              + y * SUBPIXELS;
    item->vx = sample(system, config->velocity_x) * (SUBPIXELS / 16);
    item->vy = sample(system, config->velocity_y) * (SUBPIXELS / 16);
    item->size = sample(system, config->size);
    item->lifetime = sample(system, config->lifetime);
    item->colour = colour;
  }
}
