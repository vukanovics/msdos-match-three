#ifndef ENGINE_DIAG_H_INCLUDED
#define ENGINE_DIAG_H_INCLUDED

#include <stdint.h>

#include "engine/scene.h"
#include "engine/state.h"

///
/// @brief Guest side inspection: a text log the host can read while the game
/// runs, written into the mounted directory.
///
/// Every write reopens the file and closes it again. DOSBox buffers host writes
/// until the handle closes, so a handle kept open would stay invisible on the
/// host side - which is exactly where the tooling reads it.
///
void diag_open(const char *path);

///
/// @brief Appends one line to the log.
///
void diag_line(const char *text);

///
/// @brief Accumulates per-frame timings; only the summary is written.
///
void diag_frame(uint32_t frame, uint32_t draw_us, uint32_t frame_us,
                uint32_t now_us);

///
/// @brief Writes the timing summary gathered so far.
///
void diag_timings(void);

///
/// @brief Writes the scene description and the full world state.
///
void diag_state(const scene *scene, const world_state *state);

void diag_close(void);

#endif
