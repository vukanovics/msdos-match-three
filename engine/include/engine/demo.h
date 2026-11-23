#ifndef ENGINE_DEMO_H_INCLUDED
#define ENGINE_DEMO_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///
/// @brief A recording of one run, and the means to play it back.
///
/// A demo is the input log plus the state checksum of every frame. Playing it
/// back has to reproduce every checksum; that is what "the same input gives the
/// same frames" means, and it is checkable rather than aspirational.
///
/// This only works because the engine advances the world by exactly one tick
/// per displayed frame. If the tick rate were derived from the clock, the
/// number of ticks per frame would depend on how fast the host happened to be,
/// and frame N would not mean anything in particular.
///
#define DEMO_MAX_FRAMES 20000
#define DEMO_MAX_KEYS 1024
#define DEMO_KEYS_PER_FRAME 8

typedef enum {
  DEMO_OFF,    // normal play: nothing is recorded or checked
  DEMO_RECORD, // normal play, recording keys and per-frame checksums
  DEMO_PLAY,   // input comes from the demo, not the keyboard
} demo_mode;

typedef struct {
  uint32_t frame;
  uint8_t key;
} demo_key;

typedef struct {
  demo_mode mode;
  const char *path;

  uint32_t frames; // frames recorded, or frames to play
  uint32_t keys;
  demo_key key[DEMO_MAX_KEYS];

  // Per-frame checksums: filled while recording, loaded when playing.
  uint32_t hash[DEMO_MAX_FRAMES];

  bool truncated;          // the run outgrew the buffers
  uint32_t next_key;       // PLAY: next event to deliver
  uint32_t mismatch_frame; // PLAY: first frame whose checksum differed
} demo;

///
/// @brief Prepares @p this. In DEMO_PLAY the file is read immediately.
///
bool demo_open(demo *this, demo_mode mode, const char *path, char *error,
               size_t error_size);

///
/// @brief Appends one key to the recording. Ignored once full.
///
void demo_record_key(demo *this, uint32_t frame, uint8_t key);

///
/// @brief Appends one frame's checksum. Ignored once full.
///
void demo_record_hash(demo *this, uint32_t frame, uint32_t hash);

///
/// @brief Takes up to @p max keys scheduled for @p frame.
///
/// @return how many were written to @p keys.
///
int demo_keys_at(demo *this, uint32_t frame, uint8_t *keys, int max);

///
/// @brief Compares one frame's checksum against the recording.
///
void demo_check_hash(demo *this, uint32_t frame, uint32_t hash);

///
/// @brief Finishes the recording or the playback, and describes what happened.
///
void demo_close(demo *this, char *summary, size_t summary_size);

#endif
