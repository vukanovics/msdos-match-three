#ifndef VGA_H_INCLUDED
#define VGA_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <common/rect.h>
#include <vga/planarized_image.h>

// we are only doing mode X
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

void switch_to_mode_x();

void switch_to_mode_3();

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
// clang-format off
/// @}
///
/// @defgroup crtc_vertical_retrace_end_flags CRTC Vertical Retrace Register Flags
/// @{
///
// clang-format on
extern const uint8_t CRTC_VERTICAL_RETRACE_END_PROTECT;
/// @}

void set_crtc(const crtc_register_index *index, uint8_t data);

uint8_t get_crtc(const crtc_register_index *index);

extern const size_t AC_INDEX_READ;
extern const size_t AC_INDEX_WRITE;

extern const size_t AC_DATA_READ;
extern const size_t AC_DATA_WRITE;

///
/// Opaque index identifying a AC register.
///

/// void set_crtc(const crtc_register_index *index, uint8_t data)When set to 0,
/// the Asynchronous Reset field (bit 0) commands the sequencer to
/// asynchronously clear and halt. Resetting the sequencer with this bit can
/// cause loss of video data.
///
/// Opaque index identifying a AC register.
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

void set_ac(const ac_register_index *index, uint8_t data);

uint8_t get_ac(const ac_register_index *index);

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
extern const sc_register_index SC_RESET;
extern const sc_register_index SC_CLOCKING_MODE;
extern const sc_register_index SC_MAP_MASK;
extern const sc_register_index SC_CHARACTER_MAP_SELECT;
extern const sc_register_index SC_MEMORY_MODE;
/// @}
///
/// @defgroup sc_memory_mode_flags SC Memory Mode Register Flags
/// @{
///
extern const uint8_t SC_MEMORY_MODE_CHAIN_4;
extern const uint8_t SC_MEMORY_MODE_ODD_EVEN;
extern const uint8_t SC_MEMORY_MODE_EXTENDED_MEMORY;
/// @}
///
/// @defgroup sc_reset_flags SC Reset Register Flags
/// @{
///
// clang-format off
/// @note Specification (IBM VGA XGA Technical Reference Manual, May 19th 1992, p. 2-48):
/// @code{.text}
/// When set to 0, the Synchronous Reset field (bit 1)
/// commands the sequencer to synchronously clear and
/// halt. Bits 1 and 0 must be 1 to allow the sequencer to
/// operate. To prevent the loss of data, bit 1 must be set
/// to 0 during the active display interval before changing
/// the clock selection. The clock is changed through the
/// Clocking Mode register or the Miscellaneous Output
/// register.
/// @endcode
// clang-format on
extern const uint8_t SC_RESET_SR;
// clang-format off
/// @note Specification (IBM VGA XGA Technical Reference Manual, May 19th 1992, p. 2-48):
/// @code{.text}
/// When set to 0, the Asynchronous Reset field (bit 0)
/// commands the sequencer to asynchronously clear and
/// halt. Resetting the sequencer with this bit can cause
/// loss of video data.
/// @endcode
// clang-format on
extern const uint8_t SC_RESET_ASR;
/// @}
///
/// @defgroup sc_map_mask_flags SC Map Mask Register Flags
/// @{
extern const uint8_t SC_MAP_MASK_PLANE_1;
extern const uint8_t SC_MAP_MASK_PLANE_2;
extern const uint8_t SC_MAP_MASK_PLANE_3;
extern const uint8_t SC_MAP_MASK_PLANE_4;
/// @}

void set_sc(const sc_register_index *index, uint8_t data);

uint8_t get_sc(const sc_register_index *index);

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
extern const gc_register_index GC_BIT_MASK;
/// @}

void set_gc(const gc_register_index *index, uint8_t data);

uint8_t get_gc(const gc_register_index *index);

typedef enum copy_to_framebuffer_result_e {
  COPY_TO_FRAMEBUFFER_SUCCESS,
  COPY_TO_FRAMEBUFFER_ERROR_INVALID_SOURCE
} copy_to_framebuffer_result;

///
/// @brief Loads the image from system memory to video memory.
///
/// Should only be used to load the image for the first time to an off-screen
/// section of the framebuffer. Subsequently, faster framebuffer-to-framebuffer
/// copies should be used instead.
///
/// @param source Source \ref planarized_image.
/// @param target Target \ref rect.
///
copy_to_framebuffer_result copy_to_framebuffer(planarized_image *source,
                                               rect target);

typedef enum framebuffer_copy_result_e {
  FRAMEBUFFER_COPY_SUCCESS,
  FRAMEBUFFER_COPY_ALIGNMENTS_DIFFERENT,
} framebuffer_copy_result;
///
/// @brief Copies pixels from source to target rects
///
/// Does a very fast framebuffer-to-framebuffer copy.
/// Source and target rects must have the same column alignment.
///
framebuffer_copy_result framebuffer_copy(rect source, rect target);

char read_pixel(uint16_t x, uint16_t y);

#endif
