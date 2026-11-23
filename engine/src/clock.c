#include "engine/clock.h"

#include <go32.h>
#include <pc.h>
#include <sys/farptr.h>

#define PIT_HZ 1193182u
#define PIT_PERIOD 65536u
#define BIOS_TICK_COUNTER 0x46cu

// One BIOS tick is PIT_PERIOD counter cycles; resync when the tick count says
// more than a tick of time has passed that the counter deltas did not see.
#define RESYNC_THRESHOLD_US 60000u

static uint32_t base_tick;
static uint32_t accumulated_us;
static uint16_t previous_counter;
static uint32_t last_tick;
static uint16_t last_counter;

static uint32_t bios_ticks(void) {
  return _farpeekl(_dos_ds, BIOS_TICK_COUNTER);
}

// Channel 2 drives the PC speaker, which this engine does not use, so it is
// free to be repurposed as a timer. Mode 3 (what the BIOS programs channel 0
// as) decrements the counter once per *two* input clocks, which makes the raw
// count ambiguous; mode 2 decrements once per clock, so a difference between
// two readings is simply elapsed clocks.
//
// Port 0x61 bit 0 is the channel 2 gate; the counter does not advance without
// it. Bit 1 is the speaker output and stays clear.
static void start_timer(void) {
  outportb(0x43, 0xb4); // channel 2, mode 2, binary, low then high byte
  outportb(0x42, 0x00); // divisor 65536, so the counter spans one BIOS tick
  outportb(0x42, 0x00);
  outportb(0x61, (uint8_t)((inportb(0x61) & 0xfc) | 0x01));
}

static uint16_t pit_counter(void) {
  outportb(0x43, 0x80); // latch channel 2
  uint8_t low = inportb(0x42);
  uint8_t high = inportb(0x42);
  return (uint16_t)(low | (high << 8));
}

uint32_t clock_raw_tick(void) { return last_tick; }

uint16_t clock_raw_counter(void) { return last_counter; }

void clock_init(void) {
  start_timer();
  base_tick = bios_ticks();
  previous_counter = pit_counter();
  accumulated_us = 0;
  last_tick = base_tick;
  last_counter = previous_counter;
}

uint32_t clock_us(void) {
  uint32_t tick = bios_ticks();
  uint16_t counter = pit_counter();

  // Counts elapsed since the previous call. 16 bit arithmetic makes the
  // counter reload free; the counter always counts down in mode 2.
  uint16_t delta = (uint16_t)(previous_counter - counter);
  previous_counter = counter;
  last_tick = tick;
  last_counter = counter;

  accumulated_us += (uint32_t)((uint64_t)delta * 1000000u / PIT_HZ);

  // A stall longer than one counter period loses whole reloads; fall back to
  // the BIOS tick count so time does not drift permanently.
  uint64_t ticks = (uint64_t)(tick - base_tick) * PIT_PERIOD;
  uint32_t from_ticks = (uint32_t)(ticks * 1000000u / PIT_HZ);
  if (from_ticks > accumulated_us + RESYNC_THRESHOLD_US) {
    accumulated_us = from_ticks;
  }

  return accumulated_us;
}
