#include "common/image.h"
#include "vga/planarized_image.h"
#include "vga/vga.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/nearptr.h>

#define GRID_COLUMNS 10
#define GRID_ROWS 10

int main(int argc, char **argv) {
  switch_to_mode_x();
  __djgpp_nearptr_enable();

  image *sprite = image_load("test.fun");
  if (!sprite) {
    __djgpp_nearptr_disable();
    switch_to_mode_3();
    printf("failed to load test.fun\n");
    return 1;
  }

  planarized_image *planarized = planarized_image_new(sprite);
  size sprite_size = image_get_size(sprite);

  int grid_width = GRID_COLUMNS * (int)sprite_size.w;
  int grid_height = GRID_ROWS * (int)sprite_size.h;
  int origin_x = (SCREEN_WIDTH - grid_width) / 2;
  int origin_y = (SCREEN_HEIGHT - grid_height) / 2;

  for (int row = 0; row < GRID_ROWS; row++) {
    for (int column = 0; column < GRID_COLUMNS; column++) {
      int x = origin_x + column * (int)sprite_size.w;
      int y = origin_y + row * (int)sprite_size.h;

      // copy_to_framebuffer cannot clip a partly off-screen sprite, so skip
      // cells that do not fit entirely on screen.
      if (x < 0 || y < 0 || x + (int)sprite_size.w > SCREEN_WIDTH ||
          y + (int)sprite_size.h > SCREEN_HEIGHT) {
        continue;
      }

      rect target = {
          .x = (uint32_t)x,
          .y = (uint32_t)y,
          .size = sprite_size,
      };
      copy_to_framebuffer(planarized, target);
    }
  }

  getchar();

  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return 0;
}
