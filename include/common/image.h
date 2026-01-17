#ifndef IMAGE_H_INCLUDED
#define IMAGE_H_INCLUDED

#include <common/size.h>
#include <stdint.h>

///
/// @brief Raw image data. Must be planarized before being loaded.
///
typedef struct image_s image;

image *image_new(uint8_t *data, size size);

image *image_new_test_columns(size size);

size image_get_size(image *this);

uint8_t *image_get_data(image *this);

uint8_t image_get_pixel(image *this, uint16_t x, uint16_t y);

image *image_load(const char *filename);

#endif
