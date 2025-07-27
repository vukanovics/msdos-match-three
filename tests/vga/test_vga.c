#include <stdio.h>
#include <vga/vga.h>

int crtc_set_get_mono() {
  misc_output_register misc_output = get_misc_output();
  misc_output.io_address_select = false;
  uint8_t prev = get_crtc(CRTC_START_ADDRESS_HIGH);
  set_crtc(CRTC_START_ADDRESS_HIGH, 0x05);
  uint8_t address_high = get_crtc(CRTC_START_ADDRESS_HIGH);
  set_crtc(CRTC_START_ADDRESS_HIGH, prev);
  return address_high == 0x05;
}

int crtc_set_get_color() {
  misc_output_register misc_output = get_misc_output();
  misc_output.io_address_select = true;
  uint8_t prev = get_crtc(CRTC_START_ADDRESS_HIGH);
  set_crtc(CRTC_START_ADDRESS_HIGH, 0x10);
  uint8_t address_high = get_crtc(CRTC_START_ADDRESS_HIGH);
  set_crtc(CRTC_START_ADDRESS_HIGH, prev);
  return address_high == 0x10;
}
