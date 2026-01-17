#include "common/image.h"
#include "conio.h"
#include "sys/nearptr.h"
#include "vga/planarized_image.h"
#include <stdio.h>
#include <vga/vga.h>

int crtc_set_get_mono() {
  misc_output_register misc_output = get_misc_output();
  misc_output.io_address_select = false;
  set_misc_output(misc_output);
  uint8_t prev = get_crtc(&CRTC_START_ADDRESS_HIGH);
  set_crtc(&CRTC_START_ADDRESS_HIGH, 0x05);
  uint8_t address_high = get_crtc(&CRTC_START_ADDRESS_HIGH);
  set_crtc(&CRTC_START_ADDRESS_HIGH, prev);
  return address_high == 0x05;
}

int crtc_set_get_color() {
  misc_output_register misc_output = get_misc_output();
  misc_output.io_address_select = true;
  set_misc_output(misc_output);
  uint8_t prev = get_crtc(&CRTC_START_ADDRESS_HIGH);
  set_crtc(&CRTC_START_ADDRESS_HIGH, 0x10);
  uint8_t address_high = get_crtc(&CRTC_START_ADDRESS_HIGH);
  set_crtc(&CRTC_START_ADDRESS_HIGH, prev);
  return address_high == 0x10;
}

int ac_set_get_mono() {
  misc_output_register misc_output = get_misc_output();
  misc_output.io_address_select = false;
  set_misc_output(misc_output);
  uint8_t prev = get_ac(&AC_HORIZONTAL_PEL_PANNING) & 0b00001111;
  set_ac(&AC_HORIZONTAL_PEL_PANNING, 0x05);
  uint8_t horizontal_pel_panning =
      get_ac(&AC_HORIZONTAL_PEL_PANNING) & 0b00001111;
  set_ac(&AC_HORIZONTAL_PEL_PANNING, prev);
  return horizontal_pel_panning == 0x05;
}

int ac_set_get_color() {
  misc_output_register misc_output = get_misc_output();
  misc_output.io_address_select = true;
  set_misc_output(misc_output);
  uint8_t prev = get_ac(&AC_HORIZONTAL_PEL_PANNING) & 0b00001111;
  set_ac(&AC_HORIZONTAL_PEL_PANNING, 0x06);
  uint8_t horizontal_pel_panning =
      get_ac(&AC_HORIZONTAL_PEL_PANNING) & 0b00001111;
  set_ac(&AC_HORIZONTAL_PEL_PANNING, prev);
  return horizontal_pel_panning == 0x06;
}

int sc_set_get() {
  uint8_t prev = get_sc(&SC_MAP_MASK);
  set_sc(&SC_MAP_MASK, 0x06);
  uint8_t horizontal_pel_panning = get_sc(&SC_MAP_MASK);
  set_sc(&SC_MAP_MASK, prev);
  return horizontal_pel_panning == 0x06;
}

int sc_disable_chain4() {
  set_sc(&SC_MEMORY_MODE, 0 | SC_MEMORY_MODE_CHAIN_4);
  uint8_t prev = get_sc(&SC_MEMORY_MODE);
  set_sc(&SC_MEMORY_MODE, 0 & ~SC_MEMORY_MODE_CHAIN_4);
  uint8_t current = get_sc(&SC_MEMORY_MODE);
  printf("prev=%d,curr=%d", prev, current);
  return prev != current;
}

int gc_set_get() {
  uint8_t prev = get_gc(&GC_MISC);
  set_gc(&GC_MISC, 0x08);
  uint8_t horizontal_pel_panning = get_gc(&GC_MISC);
  set_gc(&GC_MISC, prev);
  return horizontal_pel_panning == 0x08;
}

int copy_to_framebuffer_x_offset_0() {
  int result = 1;
  switch_to_mode_x();
  __djgpp_nearptr_enable();
  size size = {.h = 32, .w = 32};
  image *source = image_new_test_columns(size);
  planarized_image *planarized_image = planarized_image_new(source);
  rect target = {.x = 0, .y = 0, .size = {.h = 32, .w = 32}};
  copy_to_framebuffer(planarized_image, target);
  getch();
  char pixel_1 = read_pixel(0, 0);
  char pixel_2 = read_pixel(1, 1);
  char pixel_3 = read_pixel(2, 2);
  char pixel_4 = read_pixel(3, 3);
  for (int x = 0; x < 32; x++) {
    for (int y = 0; y < 32; y++) {
      if (image_get_pixel(source, x, y) != read_pixel(x, y)) {
        result = 0;
        goto cleanup;
      }
    }
  }

cleanup:
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  printf("pixel 1=%d\n", pixel_1);
  printf("pixel 2=%d\n", pixel_2);
  printf("pixel 3=%d\n", pixel_3);
  printf("pixel 4=%d\n", pixel_4);
  return result;
}
int copy_to_framebuffer_x_offset_1() { return 0; }
int copy_to_framebuffer_x_offset_2() { return 0; }
int copy_to_framebuffer_x_offset_3() { return 0; }
