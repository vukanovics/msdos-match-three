#include <vga/vga.h>

#include <pc.h>

const size_t INPUT_STATUS_1_READ_MONOCHROME = 0x03ba;
const size_t INPUT_STATUS_1_READ_COLOR = 0x03da;

const size_t MISC_OUTPUT_READ = 0x03cc;
const size_t MISC_OUTPUT_WRITE = 0x03c2;

const size_t CRTC_INDEX_READ_COLOR = 0x03d4;
const size_t CRTC_INDEX_WRITE_COLOR = 0x03d4;
const size_t CRTC_INDEX_READ_MONOCHROME = 0x03b4;
const size_t CRTC_INDEX_WRITE_MONOCHROME = 0x03b4;

const size_t CRTC_DATA_READ_COLOR = 0x03d5;
const size_t CRTC_DATA_WRITE_COLOR = 0x03d5;
const size_t CRTC_DATA_READ_MONOCHROME = 0x03b5;
const size_t CRTC_DATA_WRITE_MONOCHROME = 0x03b5;

struct crtc_register_index_s {
  uint8_t d;
};

const crtc_register_index CRTC_VERTICAL_TOTAL = {0x06};
const crtc_register_index CRTC_OVERFLOW = {0x07};
const crtc_register_index CRTC_MAX_SCAN_LINE = {0x09};
const crtc_register_index CRTC_START_ADDRESS_HIGH = {0x0c};
const crtc_register_index CRTC_START_ADDRESS_LOW = {0x0d};
const crtc_register_index CRTC_VERTICAL_RETRACE_START = {0x10};
const crtc_register_index CRTC_VERTICAL_RETRACE_END = {0x11};
const crtc_register_index CRTC_VERTICAL_DISPLAY_ENABLE_END = {0x12};
const crtc_register_index CRTC_OFFSET = {0x13};
const crtc_register_index CRTC_UNDERLINE_LOCATION = {0x14};
const crtc_register_index CRTC_VBLANK_START = {0x15};
const crtc_register_index CRTC_VBLANK_END = {0x16};
const crtc_register_index CRTC_MODE_CONTROL = {0x17};

const size_t AC_INDEX_READ = 0x03c0;
const size_t AC_INDEX_WRITE = 0x03c0;
const size_t AC_DATA_READ = 0x03c1;
const size_t AC_DATA_WRITE = 0x03c0;

struct ac_register_index_s {
  size_t d;
};

const ac_register_index AC_HORIZONTAL_PEL_PANNING = {0x13};

const size_t SC_INDEX_READ = 0x03c4;
const size_t SC_INDEX_WRITE = 0x03c4;
const size_t SC_DATA_READ = 0x03c5;
const size_t SC_DATA_WRITE = 0x03c5;

struct sc_register_index_s {
  size_t d;
};

const sc_register_index SC_MAP_MASK = {0x02};

const size_t GC_INDEX_READ = 0x03ce;
const size_t GC_INDEX_WRITE = 0x03ce;
const size_t GC_DATA_READ = 0x03cf;
const size_t GC_DATA_WRITE = 0x03cf;

struct gc_register_index_s {
  size_t d;
};

const gc_register_index GC_MISC = {0x06};

void switch_to_mode_x() {}

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

void set_sc(sc_register_index index, uint8_t data) {
  outportb(SC_INDEX_WRITE, index.d);
  outportb(SC_DATA_WRITE, data);
}

uint8_t get_sc(sc_register_index index) {
  outportb(SC_INDEX_WRITE, index.d);
  return inportb(SC_DATA_READ);
}

void set_gc(gc_register_index index, uint8_t data) {
  outportb(GC_INDEX_WRITE, index.d);
  outportb(GC_DATA_WRITE, data);
}

uint8_t get_gc(gc_register_index index) {
  outportb(GC_INDEX_WRITE, index.d);
  return inportb(GC_DATA_READ);
}
