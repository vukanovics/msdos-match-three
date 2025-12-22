#include <common/image.h>

#include <common/size.h>
#include <stdint.h>
#include <stdlib.h>

struct image_s {
  uint8_t *data;
  size size;
};

size image_get_size(image *this) { return this->size; }

uint8_t *image_get_data(image *this) { return this->data; }

image *image_new(uint8_t *data, size size) {
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
