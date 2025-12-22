#include <common/image.h>

#include <common/size.h>
#include <errno.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
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

error image_load(const char *filename, image *out) {

  FILE *file = fopen(filename, "r");
  if (!file) {
    return errno;
  }

  uint32_t image_width = 0;
  uint32_t image_height = 0;

  uint8_t signature[8];
  fread(signature, 1, 8, file);
  if (!png_check_sig(signature, 8)) {
    return ERR_PNG_INVALID_SIGNATURE;
  }

  png_structp png_read_struct =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_read_struct) {
    return ERR_PNG_OUT_OF_MEMORY;
  }

  png_infop png_info_struct = png_create_info_struct(png_read_struct);
  if (!png_info_struct) {
    png_destroy_read_struct(&png_read_struct, NULL, NULL);
    return ERR_PNG_OUT_OF_MEMORY;
  }

  if (setjmp(png_jmpbuf(png_read_struct))) {
    png_destroy_read_struct(&png_read_struct, &png_info_struct, NULL);
    return ERR_PNG_LOADING_FAILED;
  }

  png_init_io(png_read_struct, file);
  png_set_sig_bytes(png_read_struct, 8);
  png_read_info(png_read_struct, png_info_struct);

  int bit_depth = 0;
  int color_type = 0;

  png_get_IHDR(png_read_struct, png_info_struct, &image_width, &image_height,
               &bit_depth, &color_type, NULL, NULL, NULL);

  size_t row_bytes = png_get_rowbytes(png_read_struct, png_info_struct);
  uint8_t *image_data = malloc(row_bytes * image_height);

  return ERR_SUCCESS;
}
