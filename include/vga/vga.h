#ifndef VGA_H_INCLUDED
#define VGA_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Input Status Register 1 has different addresses depending on the I/O address
// select misc output register field (0 == monochrome, 1 == color)
static const size_t INPUT_STATUS_1_READ_MONOCHROME = 0x03ba;
static const size_t INPUT_STATUS_1_READ_COLOR = 0x03da;

typedef struct {
  bool vertical_retrace;
  bool display_enable;
} input_status_1_register;

input_status_1_register get_input_status_1();

static const size_t MISC_OUTPUT_READ = 0x03cc;
static const size_t MISC_OUTPUT_WRITE = 0x03c2;

typedef struct {
  bool vertical_sync_polarity;
  bool horizontal_sync_polarity;
  uint8_t clock_select;
  bool enable_ram;
  bool io_address_select;
} misc_output_register;

void set_misc_output(misc_output_register data);

misc_output_register get_misc_output();

// CRT controller is unique among controllers in that its register addresses are
// different between monochrome & color modes
static const size_t CRTC_INDEX_READ_COLOR = 0x03d4;
static const size_t CRTC_INDEX_WRITE_COLOR = 0x03d4;

static const size_t CRTC_INDEX_READ_MONOCHROME = 0x03b4;
static const size_t CRTC_INDEX_WRITE_MONOCHROME = 0x03b4;

static const size_t CRTC_DATA_READ_COLOR = 0x03d5;
static const size_t CRTC_DATA_WRITE_COLOR = 0x03d5;

static const size_t CRTC_DATA_READ_MONOCHROME = 0x03b5;
static const size_t CRTC_DATA_WRITE_MONOCHROME = 0x03b5;

typedef struct {
  uint8_t d;
} crtc_register_index;

// clang-format off
static const crtc_register_index CRTC_START_ADDRESS_HIGH = {0x0c};
static const crtc_register_index CRTC_START_ADDRESS_LOW = {0x0d};
static const crtc_register_index CRTC_OFFSET = {0x13};
// clang-format on

void set_crtc(crtc_register_index index, uint8_t data);

uint8_t get_crtc(crtc_register_index index);

static const size_t AC_INDEX_READ = 0x03c0;
static const size_t AC_INDEX_WRITE = 0x03c0;

static const size_t AC_DATA_READ = 0x03c1;
static const size_t AC_DATA_WRITE = 0x03c0;

typedef struct {
  size_t d;
} ac_register_index;

// clang-format off
static const ac_register_index AC_HORIZONTAL_PEL_PANNING = {0x13};
// clang-format on

void set_ac(ac_register_index index, uint8_t data);

uint8_t get_ac(ac_register_index index);

static const size_t SC_INDEX_READ = 0x03c4;
static const size_t SC_INDEX_WRITE = 0x03c4;

static const size_t SC_DATA_READ = 0x03c5;
static const size_t SC_DATA_WRITE = 0x03c5;

typedef struct {
  size_t d;
} sc_register_index;

// clang-format off
static const sc_register_index SC_MAP_MASK = {0x02};
// clang-format on

void set_sc(sc_register_index index, uint8_t data);

uint8_t get_sc(sc_register_index index);

static const size_t GC_INDEX_READ = 0x03ce;
static const size_t GC_INDEX_WRITE = 0x03ce;

static const size_t GC_DATA_READ = 0x03cf;
static const size_t GC_DATA_WRITE = 0x03cf;

typedef struct {
  size_t d;
} gc_register_index;

// clang-format off
static const gc_register_index GC_MISC = {0x06};
// clang-format on

void set_gc(gc_register_index index, uint8_t data);

uint8_t get_gc(gc_register_index index);

#endif
