#include <vga/vga.h>

#include <pc.h>

void set_crtc(crtc_register_index index, uint8_t data) {
  misc_output_register misc_output = get_misc_output();
  if (misc_output.io_address_select) {
    outportb(CRTC_INDEX_WRITE_COLOR, index.d);
    outportb(CRTC_DATA_WRITE_COLOR, data);
  } else {
    outportb(CRTC_INDEX_WRITE_MONOCHROME, index.d);
    outportb(CRTC_DATA_WRITE_MONOCHROME, data);
  }
}

uint8_t get_crtc(crtc_register_index index) {
  misc_output_register misc_output = get_misc_output();
  if (misc_output.io_address_select) {
    outportb(CRTC_INDEX_WRITE_COLOR, index.d);
    return inportb(CRTC_DATA_READ_COLOR);
  } else {
    outportb(CRTC_INDEX_WRITE_MONOCHROME, index.d);
    return inportb(CRTC_DATA_READ_MONOCHROME);
  }
}

void set_ac(ac_register_index index, uint8_t data) {
  // reading the input status register 1 selects the address register (if the
  // data register was selected) - ref. IBM VGA XGA Technical Reference Manual
  // p. 2-89
  get_input_status_1();
  outportb(AC_INDEX_WRITE, index.d);
  outportb(AC_DATA_WRITE, data);
}

uint8_t get_ac(ac_register_index index) {
  get_input_status_1();
  outportb(AC_INDEX_WRITE, index.d);
  return inportb(AC_DATA_READ);
}

input_status_1_register get_input_status_1() {
  uint8_t buffer = 0;
  misc_output_register misc_output = get_misc_output();
  if (misc_output.io_address_select) {
    uint8_t buffer = inportb(INPUT_STATUS_1_READ_COLOR);
  } else {
    uint8_t buffer = inportb(INPUT_STATUS_1_READ_MONOCHROME);
  }

  input_status_1_register out;

  out.vertical_retrace = (buffer & 0b00001000) >> 3;
  out.display_enable = (buffer & 0b00000001);

  return out;
}

void set_misc_output(misc_output_register data) {
  uint8_t buffer = data.vertical_sync_polarity << 7 |
                   data.horizontal_sync_polarity << 6 | 0 << 5 | 0 << 4 |
                   data.clock_select << 3 | data.enable_ram << 1 |
                   data.io_address_select;
  outportb(MISC_OUTPUT_WRITE, buffer);
}

misc_output_register get_misc_output() {
  uint8_t buffer = inportb(MISC_OUTPUT_READ);

  misc_output_register out;

  out.vertical_sync_polarity = buffer >> 7;
  out.horizontal_sync_polarity = (buffer & 0b01000000) >> 6;
  out.clock_select = (buffer & 0b00001100) >> 2;
  out.enable_ram = (buffer & 0b00000010) >> 1;
  out.io_address_select = (buffer & 0b00000001);

  return out;
}
