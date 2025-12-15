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
