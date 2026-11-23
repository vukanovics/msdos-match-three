#ifndef ENGINE_CLOCK_H_INCLUDED
#define ENGINE_CLOCK_H_INCLUDED

#include <stdint.h>

///
/// @brief Monotonic microsecond clock.
///
/// Integrates the PIT channel 0 counter (1.193182 MHz), which is fine grained
/// and independent of how an emulator happens to align that counter with the
/// BIOS tick. Call it at least once every 50 ms or a counter reload can be
/// missed. The value wraps after roughly 71 minutes; compare readings with
/// unsigned subtraction so the wrap is harmless.
///
void clock_init(void);

uint32_t clock_us(void);

///
/// @brief Raw readings, for diagnosing the clock itself.
///
uint32_t clock_raw_tick(void);

uint16_t clock_raw_counter(void);

#endif
