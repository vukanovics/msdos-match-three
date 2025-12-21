#include "common/image.h"
#include <stdlib.h>
#include <vga/planarized_image.h>

#include <common/size.h>
#include <stdint.h>

struct planarized_image_s {
  uint8_t *plane_1_data;
  uint8_t *plane_2_data;
  uint8_t *plane_3_data;
  uint8_t *plane_4_data;
  size size;
};

planarized_image *planarized_image_new(image *source) {
  size source_size = image_get_size(source);
  uint8_t *source_data = image_get_data(source);
  size_t bytes_per_plane = source_size.w * source_size.h / 4;
  uint8_t *plane_1_data = malloc(bytes_per_plane);
  uint8_t *plane_2_data = malloc(bytes_per_plane);
  uint8_t *plane_3_data = malloc(bytes_per_plane);
  uint8_t *plane_4_data = malloc(bytes_per_plane);
  for (size_t y = 0; y < source_size.h; y++) {
    for (size_t x = 0; x < source_size.w; x++) {
      switch (x % 4) {
      case 0:
        plane_1_data[y * source_size.w / 4 + x / 4] =
            source_data[y * source_size.w + x];
        break;
      case 1:
        plane_2_data[y * source_size.w / 4 + x / 4] =
            source_data[y * source_size.w + x];
        break;
      case 2:
        plane_3_data[y * source_size.w / 4 + x / 4] =
            source_data[y * source_size.w + x];
        break;
      case 3:
        plane_4_data[y * source_size.w / 4 + x / 4] =
            source_data[y * source_size.w + x];
        break;
      }
    }
  }

  planarized_image *this = malloc(sizeof(planarized_image));
  this->plane_1_data = plane_1_data;
  this->plane_2_data = plane_2_data;
  this->plane_3_data = plane_3_data;
  this->plane_4_data = plane_4_data;
  this->size = source_size;

  return this;
}

uint8_t *planarized_image_get_plane_1_data(planarized_image *this) {
  return this->plane_1_data;
}

uint8_t *planarized_image_get_plane_2_data(planarized_image *this) {
  return this->plane_2_data;
}

uint8_t *planarized_image_get_plane_3_data(planarized_image *this) {
  return this->plane_3_data;
}

uint8_t *planarized_image_get_plane_4_data(planarized_image *this) {
  return this->plane_4_data;
}
