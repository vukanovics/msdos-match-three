#ifndef ENGINE_PARTICLE_CONFIG_H_INCLUDED
#define ENGINE_PARTICLE_CONFIG_H_INCLUDED

typedef struct {
  int min;
  int max;
} particle_range;

typedef struct {
  particle_range count;
  particle_range x_percent;
  particle_range y_percent;
  particle_range velocity_x;
  particle_range velocity_y;
  particle_range colour;
  particle_range size;
  particle_range lifetime;
  int gravity;
} particle_config;

#endif
