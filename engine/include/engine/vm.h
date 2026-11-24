#ifndef ENGINE_VM_H_INCLUDED
#define ENGINE_VM_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Small enough to keep the VM allocation free and deterministic.
#define VM_STACK_SIZE 32
#define VM_CALL_DEPTH 16

///
/// @brief Stack machine for compiled behaviour programs.
///
/// The world state is a flat table of 32-bit slots that the program reads and
/// writes through named or computed slot indices; there is no allocation, no
/// garbage collector and no recursion, so a program always terminates.
///
/// Three entry points, matching the engine's evaluation model: @c init builds
/// the world once, @c tick advances it by one fixed step, and @c key applies one
/// input event. @c animation_end is called with the entity index when a
/// non-looping animation reaches its last cel, which is how a program can say
/// "play this once, then go back to idle" without tracking it itself.
///
typedef struct {
  const uint8_t *code;
  uint32_t code_size;
  uint32_t init_entry;
  uint32_t tick_entry;
  uint32_t key_entry;
  uint32_t animation_end_entry;

  int32_t *slots;
  uint16_t slot_count;

  // Not part of the slot table: only the values the generator has produced end
  // up in slots, and those are what determinate replays depend on.
  uint32_t random_state;

  int32_t stack[VM_STACK_SIZE];
  int stack_size;
  uint32_t returns[VM_CALL_DEPTH];
  int return_depth;

  const char *error;
} vm;

///
/// @brief Validates a compiled program against the loaded scene.
///
/// The slot count and layout fingerprint come from the scene the compiler read.
///
bool vm_load(vm *this, const uint8_t *bytes, size_t size, int32_t *slots,
             uint16_t slot_count, uint32_t fingerprint, uint32_t seed);

///
/// @brief Runs the init entry point. Call once, before the first frame.
///
bool vm_init(vm *this);

///
/// @brief Runs the tick entry point: one fixed step of the world.
///
bool vm_tick(vm *this);

///
/// @brief Runs the key entry point with @p key as its argument.
///
bool vm_key(vm *this, int32_t key);

///
/// @brief Runs the animation-end entry point with @p entity as its argument.
///
bool vm_animation_end(vm *this, int32_t entity);

///
/// @brief The generator state, so replays can be checked as a whole.
///
uint32_t vm_random_state(const vm *this);

#endif
