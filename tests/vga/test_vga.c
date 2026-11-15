#include "common/image.h"
#include "log.h"
#include "sys/nearptr.h"
#include "vga/planarized_image.h"
#include <stdio.h>
#include <stdlib.h>
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
  test_log("prev=%d,curr=%d\n", prev, current);
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
  test_log("pixel 1=%d\n", pixel_1);
  test_log("pixel 2=%d\n", pixel_2);
  test_log("pixel 3=%d\n", pixel_3);
  test_log("pixel 4=%d\n", pixel_4);
  return result;
}
int copy_to_framebuffer_x_offset_1() { return 0; }
int copy_to_framebuffer_x_offset_2() { return 0; }
int copy_to_framebuffer_x_offset_3() { return 0; }

// A pattern that varies in both x and y, so that a copy which lands on the
// wrong row is detected (a columns-only pattern would not catch that).
static image *new_framebuffer_copy_test_image(size test_size) {
  uint8_t *data = malloc(test_size.w * test_size.h);
  for (uint32_t y = 0; y < test_size.h; y++) {
    for (uint32_t x = 0; x < test_size.w; x++) {
      data[y * test_size.w + x] = (uint8_t)(x + y * test_size.w);
    }
  }
  return image_new(data, test_size);
}

// Compares the framebuffer pixels at `target` against `expected`, starting at
// column `expected_x` in the source image.
static int framebuffer_matches_image(image *expected, uint16_t expected_x,
                                     rect target) {
  for (uint16_t x = 0; x < target.size.w; x++) {
    for (uint16_t y = 0; y < target.size.h; y++) {
      if (image_get_pixel(expected, expected_x + x, y) !=
          read_pixel(target.x + x, target.y + y)) {
        return 0;
      }
    }
  }
  return 1;
}

// Places the test image at the aligned location (0, 0), then copies a sub-rect
// starting at `source_x` to `target` and verifies every pixel. Keeping the
// source aligned means a failure isolates framebuffer_copy() rather than the
// (separately unfinished) unaligned path of copy_to_framebuffer().
static int run_framebuffer_copy(uint16_t source_x, rect target) {
  size test_size = {.h = 32, .w = 32};
  image *source_image = new_framebuffer_copy_test_image(test_size);
  planarized_image *planarized = planarized_image_new(source_image);

  rect placement = {.x = 0, .y = 0, .size = test_size};
  copy_to_framebuffer(planarized, placement);

  rect source = {.x = source_x, .y = 0, .size = target.size};
  if (framebuffer_copy(source, target) != FRAMEBUFFER_COPY_SUCCESS) {
    return 0;
  }

  return framebuffer_matches_image(source_image, source_x, target);
}

int framebuffer_copy_x_offset_0() {
  switch_to_mode_x();
  __djgpp_nearptr_enable();
  rect target = {.x = 160, .y = 40, .size = {.h = 32, .w = 28}};
  int result = run_framebuffer_copy(0, target);
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return result;
}

int framebuffer_copy_x_offset_1() {
  switch_to_mode_x();
  __djgpp_nearptr_enable();
  rect target = {.x = 161, .y = 40, .size = {.h = 32, .w = 28}};
  int result = run_framebuffer_copy(1, target);
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return result;
}

int framebuffer_copy_x_offset_2() {
  switch_to_mode_x();
  __djgpp_nearptr_enable();
  rect target = {.x = 162, .y = 40, .size = {.h = 32, .w = 28}};
  int result = run_framebuffer_copy(2, target);
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return result;
}

int framebuffer_copy_x_offset_3() {
  switch_to_mode_x();
  __djgpp_nearptr_enable();
  rect target = {.x = 163, .y = 40, .size = {.h = 32, .w = 28}};
  int result = run_framebuffer_copy(3, target);
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return result;
}

int framebuffer_copy_y_offset() {
  switch_to_mode_x();
  __djgpp_nearptr_enable();
  rect target = {.x = 0, .y = 120, .size = {.h = 32, .w = 28}};
  int result = run_framebuffer_copy(0, target);
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return result;
}
