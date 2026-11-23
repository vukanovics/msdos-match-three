#include "common/image.h"
#include "vga/planarized_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vga/vga.h>

#include <dpmi.h>
#include <pc.h>
#include <sys/nearptr.h>

void *const VIDEO_MEMORY_START = (void *)0xa0000;

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

const uint8_t CRTC_VERTICAL_RETRACE_END_PROTECT = 0b10000000;

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

const sc_register_index SC_RESET = {0x00};
const sc_register_index SC_CLOCKING_MODE = {0x01};
const sc_register_index SC_MAP_MASK = {0x02};
const sc_register_index SC_CHARACTER_MAP_SELECT = {0x03};
const sc_register_index SC_MEMORY_MODE = {0x04};

const uint8_t SC_MEMORY_MODE_CHAIN_4 = 0b00001000;
const uint8_t SC_MEMORY_MODE_ODD_EVEN = 0b00000100;
const uint8_t SC_MEMORY_MODE_EXTENDED_MEMORY = 0b00000010;

const uint8_t SC_RESET_ASR = 0b00000001;
const uint8_t SC_RESET_SR = 0b00000010;

const uint8_t SC_MAP_MASK_PLANE_1 = 0b00000001;
const uint8_t SC_MAP_MASK_PLANE_2 = 0b00000010;
const uint8_t SC_MAP_MASK_PLANE_3 = 0b00000100;
const uint8_t SC_MAP_MASK_PLANE_4 = 0b00001000;

const size_t GC_INDEX_READ = 0x03ce;
const size_t GC_INDEX_WRITE = 0x03ce;
const size_t GC_DATA_READ = 0x03cf;
const size_t GC_DATA_WRITE = 0x03cf;

struct gc_register_index_s {
  size_t d;
};

const gc_register_index GC_READ_MAP_SELECT = {0x04};
const gc_register_index GC_MODE = {0x05};
const gc_register_index GC_MISC = {0x06};
const gc_register_index GC_BIT_MASK = {0x08};

// The CRTC start-address registers take a byte offset in Mode X.
#define PAGE_ADDRESS_UNIT 1

static size_t drawing_page_base = 0;

void set_draw_page(size_t base) { drawing_page_base = base; }

size_t screen_pos_to_memory_pos(uint16_t x, uint16_t y) {
  return drawing_page_base + y * SCREEN_WIDTH / 4 + x / 4;
}

void set_display_start(size_t base) {
  uint16_t address = (uint16_t)(base / PAGE_ADDRESS_UNIT);

  // The CRTC latches the start address at the beginning of vertical retrace, so
  // program it as that retrace begins and the flip takes effect for the frame
  // that is about to start; the caller can then draw into the page that just
  // stopped being displayed.
  //
  // Bit 3 is vertical sync on real hardware, but DOSBox derives the status
  // register from its own frame clock and never raises that bit for this mode,
  // while bit 0 (blanking) is raised by both. Accepting either ends the wait at
  // the same instant on hardware and makes the poll work in the emulator. The
  // loop is bounded so a display that reports nothing cannot hang the engine.
  for (uint32_t spins = 0; spins < 50000; spins++) {
    input_status_1_register status = get_input_status_1();
    if (status.vertical_retrace || status.display_enable) {
      break;
    }
  }

  set_crtc(&CRTC_START_ADDRESS_HIGH, (uint8_t)(address >> 8));
  set_crtc(&CRTC_START_ADDRESS_LOW, (uint8_t)(address & 0xff));
}

copy_to_framebuffer_result copy_to_framebuffer(planarized_image *source,
                                               rect target) {

  uint32_t x_offset_per_plane = target.x / 4;
  uint32_t screen_width_per_plane = SCREEN_WIDTH / 4;
  uint32_t target_width_per_plane = target.size.w / 4;

  for (size_t y = 0; y < target.size.h; y++) {

    uint16_t alignment = target.x % 4;

    size_t screen_y = y + target.y;
    void *plane_line_start = VIDEO_MEMORY_START + __djgpp_conventional_base +
                             drawing_page_base + // adjust page
                             screen_y * screen_width_per_plane + // adjust row
                             x_offset_per_plane; // adjust column
    size_t plane_data_offset = y * target_width_per_plane;

    switch (alignment) {
    case 0:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_1);
      break;
    case 1:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_2);
      break;
    case 2:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_3);
      break;
    case 3:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_4);
      break;
    }

    memcpy(plane_line_start + ((alignment > 3) ? 1 : 0),
           planarized_image_get_plane_1_data(source) + plane_data_offset,
           target_width_per_plane);

    switch (alignment) {
    case 0:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_2);
      break;
    case 1:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_3);
      break;
    case 2:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_4);
      break;
    case 3:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_1);
      break;
    }

    memcpy(plane_line_start + ((alignment > 2) ? 1 : 0),
           planarized_image_get_plane_2_data(source) + plane_data_offset,
           target_width_per_plane);

    switch (alignment) {
    case 0:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_3);
      break;
    case 1:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_4);
      break;
    case 2:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_1);
      break;
    case 3:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_2);
      break;
    }

    memcpy(plane_line_start + ((alignment > 1) ? 1 : 0),
           planarized_image_get_plane_3_data(source) + plane_data_offset,
           target_width_per_plane);

    switch (alignment) {
    case 0:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_4);
      break;
    case 1:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_1);
      break;
    case 2:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_2);
      break;
    case 3:
      set_sc(&SC_MAP_MASK, SC_MAP_MASK_PLANE_3);
      break;
    }

    memcpy(plane_line_start + ((alignment > 0) ? 1 : 0),
           planarized_image_get_plane_4_data(source) + plane_data_offset,
           target_width_per_plane);
  }
  return COPY_TO_FRAMEBUFFER_SUCCESS;
}

framebuffer_copy_result framebuffer_copy(rect source, rect target) {
  // the block loop below computes source.size.w / 4 - 1, which would underflow
  // for a width that is not strictly greater than 4
  if (source.size.w <= 4) {
    return FRAMEBUFFER_COPY_WIDTH_TOO_SMALL;
  }

  uint16_t source_alignment = source.x % 4;
  uint16_t target_alignment = target.x % 4;

  if (source_alignment != target_alignment) {
    return FRAMEBUFFER_COPY_ALIGNMENTS_DIFFERENT;
  }

  // Write mode 1 writes the latched planes to the enabled planes. The default
  // write mode 0 would instead write the single CPU-read byte to every enabled
  // plane, which is why the per-plane latch copy below needs mode 1.
  uint8_t gc_mode = get_gc(&GC_MODE);
  set_gc(&GC_MODE, (gc_mode & ~0b00000011) | 0b00000001);
  set_gc(&GC_BIT_MASK, 0xff);

  const uint8_t all_planes =
      SC_MAP_MASK_PLANE_1 | SC_MAP_MASK_PLANE_2 | SC_MAP_MASK_PLANE_3 |
      SC_MAP_MASK_PLANE_4;

  uint16_t source_group = source.x / 4;
  uint16_t target_group = target.x / 4;
  uint16_t group_count = source.size.w / 4;

  // With a non-zero alignment the rect spans one extra (partial) column
  // group: the first group holds planes [alignment..3] and the last holds
  // planes [0..alignment-1].
  if (source_alignment != 0) {
    group_count++;
  }

  for (uint16_t row = 0; row < source.size.h; row++) {
    uint16_t source_y = row + source.y;
    uint16_t target_y = row + target.y;

    for (uint16_t group = 0; group < group_count; group++) {
      uint8_t mask = all_planes;
      if (source_alignment != 0 && group == 0) {
        mask = (uint8_t)((0x0f << source_alignment) & 0x0f);
      } else if (source_alignment != 0 && group == group_count - 1) {
        mask = (uint8_t)((1u << source_alignment) - 1u);
      }

      set_sc(&SC_MAP_MASK, mask);

      uint16_t source_block_x = (uint16_t)((source_group + group) * 4);
      uint16_t target_block_x = (uint16_t)((target_group + group) * 4);

      uint16_t source_block =
          screen_pos_to_memory_pos(source_block_x, source_y);
      uint16_t target_block =
          screen_pos_to_memory_pos(target_block_x, target_y);

      memcpy(target_block + __djgpp_conventional_base + VIDEO_MEMORY_START,
             source_block + __djgpp_conventional_base + VIDEO_MEMORY_START, 1);
    }
  }

  set_gc(&GC_MODE, gc_mode);
  return FRAMEBUFFER_COPY_SUCCESS;
}

void switch_to_mode_13() {
  __dpmi_regs regs;
  regs.x.ax = 0x13;
  __dpmi_int(0x10, &regs);
}

void switch_to_mode_3() {
  __dpmi_regs regs;
  regs.x.ax = 0x03;
  __dpmi_int(0x10, &regs);
}

void switch_to_mode_x() {
  switch_to_mode_13();
  set_sc(&SC_MEMORY_MODE,
         SC_MEMORY_MODE_EXTENDED_MEMORY | SC_MEMORY_MODE_ODD_EVEN);
  set_sc(&SC_RESET, SC_RESET_ASR);

  misc_output_register data = {.vertical_sync_polarity = true,
                               .horizontal_sync_polarity = true,
                               .clock_select = 0,
                               .io_address_select = true,
                               .enable_ram = true};

  set_misc_output(data);

  set_sc(&SC_RESET, SC_RESET_SR | SC_RESET_ASR);

  set_crtc(&CRTC_VERTICAL_RETRACE_END, get_crtc(&CRTC_VERTICAL_RETRACE_END) &
                                           ~CRTC_VERTICAL_RETRACE_END_PROTECT);

  set_crtc(&CRTC_VERTICAL_TOTAL, 0x0d);
  set_crtc(&CRTC_OVERFLOW, 0x3e);
  set_crtc(&CRTC_MAX_SCAN_LINE, 0x41);
  set_crtc(&CRTC_VERTICAL_RETRACE_START, 0xea);
  set_crtc(&CRTC_VERTICAL_RETRACE_END, 0xac);
  set_crtc(&CRTC_VERTICAL_DISPLAY_ENABLE_END, 0xdf);
  set_crtc(&CRTC_UNDERLINE_LOCATION, 0x00);
  set_crtc(&CRTC_VBLANK_START, 0xe7);
  set_crtc(&CRTC_VBLANK_END, 0x06);
  set_crtc(&CRTC_MODE_CONTROL, 0xe3);

  set_sc(&SC_MAP_MASK, 0b00001000);
}

void set_crtc(const crtc_register_index *index, uint8_t data) {
  misc_output_register misc_output = get_misc_output();
  if (misc_output.io_address_select) {
    outportb(CRTC_INDEX_WRITE_COLOR, index->d);
    outportb(CRTC_DATA_WRITE_COLOR, data);
  } else {
    outportb(CRTC_INDEX_WRITE_MONOCHROME, index->d);
    outportb(CRTC_DATA_WRITE_MONOCHROME, data);
  }
}

uint8_t get_crtc(const crtc_register_index *index) {
  misc_output_register misc_output = get_misc_output();
  if (misc_output.io_address_select) {
    outportb(CRTC_INDEX_WRITE_COLOR, index->d);
    return inportb(CRTC_DATA_READ_COLOR);
  } else {
    outportb(CRTC_INDEX_WRITE_MONOCHROME, index->d);
    return inportb(CRTC_DATA_READ_MONOCHROME);
  }
}

void set_ac(const ac_register_index *index, uint8_t data) {
  // reading the input status register 1 selects the address register (if the
  // data register was selected) - ref. IBM VGA XGA Technical Reference Manual
  // p. 2-89
  get_input_status_1();
  outportb(AC_INDEX_WRITE, index->d);
  outportb(AC_DATA_WRITE, data);
}

uint8_t get_ac(const ac_register_index *index) {
  get_input_status_1();
  outportb(AC_INDEX_WRITE, index->d);
  return inportb(AC_DATA_READ);
}

input_status_1_register get_input_status_1() {
  uint8_t buffer = 0;
  misc_output_register misc_output = get_misc_output();
  if (misc_output.io_address_select) {
    buffer = inportb(INPUT_STATUS_1_READ_COLOR);
  } else {
    buffer = inportb(INPUT_STATUS_1_READ_MONOCHROME);
  }

  input_status_1_register out;

  out.vertical_retrace = (buffer & 0b00001000) >> 3;
  out.display_enable = (buffer & 0b00000001);

  return out;
}

void set_misc_output(misc_output_register data) {
  uint8_t buffer = data.vertical_sync_polarity << 7 |
                   data.horizontal_sync_polarity << 6 | 0 << 5 | 0 << 4 |
                   data.clock_select << 2 | data.enable_ram << 1 |
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

void set_sc(const sc_register_index *index, uint8_t data) {
  outportb(SC_INDEX_WRITE, index->d);
  outportb(SC_DATA_WRITE, data);
}

uint8_t get_sc(const sc_register_index *index) {
  outportb(SC_INDEX_WRITE, index->d);
  return inportb(SC_DATA_READ);
}

void set_gc(const gc_register_index *index, uint8_t data) {
  outportb(GC_INDEX_WRITE, index->d);
  outportb(GC_DATA_WRITE, data);
}

uint8_t get_gc(const gc_register_index *index) {
  outportb(GC_INDEX_WRITE, index->d);
  return inportb(GC_DATA_READ);
}

uint8_t read_pixel(uint16_t x, uint16_t y) {
  uint16_t alignment = x % 4;
  size_t memory_position = screen_pos_to_memory_pos(x - alignment, y);

  // CPU reads of planar VRAM return the plane selected by the Graphics
  // Controller Read Map Select register; the Sequencer Map Mask only gates
  // writes.
  set_gc(&GC_READ_MAP_SELECT, (uint8_t)alignment);

  uint8_t result = 0;
  memcpy(&result,
         (__djgpp_conventional_base + VIDEO_MEMORY_START + memory_position), 1);
  return result;
}

void write_pixel(uint16_t x, uint16_t y, uint8_t color) {
  uint16_t alignment = x % 4;
  size_t memory_position = screen_pos_to_memory_pos(x - alignment, y);

  // Writes are gated by the Sequencer Map Mask; enable only this pixel's plane
  // and store the colour byte.
  set_sc(&SC_MAP_MASK, (uint8_t)(1 << alignment));
  memcpy(VIDEO_MEMORY_START + __djgpp_conventional_base + memory_position,
         &color, 1);
}

void set_palette(const uint8_t *rgb, uint16_t count) {
  for (uint16_t index = 0; index < count; index++) {
    outportb(0x03c8, (uint8_t)index);
    outportb(0x03c9, rgb[index * 3 + 0] >> 2);
    outportb(0x03c9, rgb[index * 3 + 1] >> 2);
    outportb(0x03c9, rgb[index * 3 + 2] >> 2);
  }
}
