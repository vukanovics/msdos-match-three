#ifndef VGA_H_INCLUDED
#define VGA_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void switch_to_mode_x();

// Input Status Register 1 has different addresses depending on the I/O address
// select misc output register field (0 == monochrome, 1 == color)
extern const size_t INPUT_STATUS_1_READ_MONOCHROME;
extern const size_t INPUT_STATUS_1_READ_COLOR;

typedef struct {
  bool vertical_retrace;
  bool display_enable;
} input_status_1_register;

input_status_1_register get_input_status_1();

extern const size_t MISC_OUTPUT_READ;
extern const size_t MISC_OUTPUT_WRITE;

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
extern const size_t CRTC_INDEX_READ_COLOR;
extern const size_t CRTC_INDEX_WRITE_COLOR;

extern const size_t CRTC_INDEX_READ_MONOCHROME;
extern const size_t CRTC_INDEX_WRITE_MONOCHROME;

extern const size_t CRTC_DATA_READ_COLOR;
extern const size_t CRTC_DATA_WRITE_COLOR;

extern const size_t CRTC_DATA_READ_MONOCHROME;
extern const size_t CRTC_DATA_WRITE_MONOCHROME;

///
/// Opaque index identifying a CRTC register.
///
/// Available values are listed in @ref crtc_registers.
///
typedef struct crtc_register_index_s crtc_register_index;

///
/// @defgroup crtc_registers CRTC Register Indicies
/// Typed indicies for the CRTC register.
/// @{
///
extern const crtc_register_index CRTC_VERTICAL_TOTAL;
extern const crtc_register_index CRTC_OVERFLOW;
extern const crtc_register_index CRTC_MAX_SCAN_LINE;
extern const crtc_register_index CRTC_START_ADDRESS_HIGH;
extern const crtc_register_index CRTC_START_ADDRESS_LOW;
extern const crtc_register_index CRTC_VERTICAL_RETRACE_START;
extern const crtc_register_index CRTC_VERTICAL_RETRACE_END;
extern const crtc_register_index CRTC_VERTICAL_DISPLAY_ENABLE_END;
extern const crtc_register_index CRTC_OFFSET;
extern const crtc_register_index CRTC_UNDERLINE_LOCATION;
extern const crtc_register_index CRTC_VBLANK_START;
extern const crtc_register_index CRTC_VBLANK_END;
extern const crtc_register_index CRTC_MODE_CONTRO;
/// @}

void set_crtc(crtc_register_index index, uint8_t data);

uint8_t get_crtc(crtc_register_index index);

extern const size_t AC_INDEX_READ;
extern const size_t AC_INDEX_WRITE;

extern const size_t AC_DATA_READ;
extern const size_t AC_DATA_WRITE;

///
/// Opaque index identifying a AC register.
///
/// Available values are listed in @ref ac_registers.
///
typedef struct ac_register_index_s ac_register_index;

///
/// @defgroup ac_registers AC Register Indicies
/// Typed indicies for the AC register.
/// @{
///
extern const ac_register_index AC_HORIZONTAL_PEL_PANNING;
/// @}

void set_ac(ac_register_index index, uint8_t data);

uint8_t get_ac(ac_register_index index);

extern const size_t SC_INDEX_READ;
extern const size_t SC_INDEX_WRITE;

extern const size_t SC_DATA_READ;
extern const size_t SC_DATA_WRITE;

///
/// Opaque index identifying a SC register.
///
/// Available values are listed in @ref sc_registers.
///
typedef struct sc_register_index_s sc_register_index;

///
/// @defgroup sc_registers SC Register Indicies
/// Typed indicies for the SC register.
/// @{
///
extern const sc_register_index SC_MAP_MASK;
/// @}

void set_sc(sc_register_index index, uint8_t data);

uint8_t get_sc(sc_register_index index);

extern const size_t GC_INDEX_READ;
extern const size_t GC_INDEX_WRITE;

extern const size_t GC_DATA_READ;
extern const size_t GC_DATA_WRITE;

///
/// Opaque index identifying a GC register.
///
/// Available values are listed in @ref gc_registers.
///
typedef struct gc_register_index_s gc_register_index;
///
/// @defgroup gc_registers GC Register Indicies
/// Typed indicies for the GC register.
/// @{
///
extern const gc_register_index GC_MISC;
/// @}

void set_gc(gc_register_index index, uint8_t data);

uint8_t get_gc(gc_register_index index);

#endif
