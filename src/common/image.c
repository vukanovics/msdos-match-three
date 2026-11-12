#include <common/image.h>

#include <assert.h>
#include <common/size.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct image_s {
  size size;
  uint8_t *data;
};

size image_get_size(image *this) { return this->size; }

uint8_t *image_get_data(image *this) { return this->data; }

image *image_new(uint8_t *data, size size) {
  assert(size.w % 4 == 0);
  image *this = malloc(sizeof(image));
  this->data = data;
  this->size = size;
  return this;
}

image *image_new_test_columns(size size) {
  uint8_t *data = malloc(size.w * size.h);
  for (size_t y = 0; y < size.h; y++) {
    for (size_t x = 0; x < size.w; x++) {
      data[y * size.w + x] = x;
    }
  }
  return image_new(data, size);
}

image *image_load(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (!file) {
    printf("failed to open file\n");
    return NULL;
  }
  uint16_t width = 0;
  uint16_t height = 0;
  fread(&width, sizeof(uint16_t), 1, file);
  fread(&height, sizeof(uint16_t), 1, file);
  uint8_t *data = malloc(width * height);
  fread(data, width * height, 1, file);
  fclose(file);
  size size = {
      .w = width,
      .h = height,
  };

  return image_new(data, size);
}

uint8_t image_get_pixel(image *this, uint16_t x, uint16_t y) {
  return this->data[y * this->size.w + x];
}
