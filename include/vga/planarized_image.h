#ifndef PLANARIZED_IMAGE_H_INCLUDED
#define PLANARIZED_IMAGE_H_INCLUDED

#include <common/image.h>
#include <stdint.h>

///
/// @brief Image with data split into four Mode X VGA planes.
///
typedef struct planarized_image_s planarized_image;

planarized_image *planarized_image_new(image *source);

uint8_t *planarized_image_get_plane_1_data(planarized_image *this);
uint8_t *planarized_image_get_plane_2_data(planarized_image *this);
uint8_t *planarized_image_get_plane_3_data(planarized_image *this);
uint8_t *planarized_image_get_plane_4_data(planarized_image *this);

#endif
