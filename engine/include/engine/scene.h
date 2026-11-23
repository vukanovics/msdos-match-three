#ifndef ENGINE_SCENE_H_INCLUDED
#define ENGINE_SCENE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "engine/slots.h"

#define SCENE_NAME_MAX 32
#define SCENE_MAX_ENTITIES 256
#define SCENE_MAX_GLOBALS 256

// Sheets a scene may name, cels it may hold in total, and sprites it may name.
// The engine's sprite capacity is whatever the scene is allowed to ask for, so
// these are the only place the limits are written down.
#define SCENE_MAX_SHEETS 8
#define SCENE_MAX_CELS 128
#define SCENE_MAX_SPRITES 64

#define SCENE_MAX_SLOTS \
  (SCENE_MAX_ENTITIES * ENTITY_STRIDE + SCENE_MAX_GLOBALS)

///
/// @brief A named slot the scene declares, with its initial value.
///
/// Globals follow the entity pool, in declaration order, which is what lets the
/// compiler turn '@cursor-row' into a constant offset.
///
typedef struct {
  char name[SCENE_NAME_MAX];
  int32_t initial;
} scene_global;

//
/// @brief One sprite sheet the scene names: a logical name, the file to load,
/// and how many cels it must hold.
///
/// The count is declared here rather than read from the file so the behaviour
/// compiler can assign every sprite a handle without touching the assets. The
/// loader checks the file against it, so the two cannot drift apart silently -
/// the same arrangement the slot count uses.
///
typedef struct {
  char name[SCENE_NAME_MAX];
  char path[SCENE_NAME_MAX];
  int cel_count;
} scene_sheet;

///
/// @brief A sprite the scene names, resolved to a sheet and a cel.
///
/// The behaviour program refers to these as $name. Handles are assigned by
/// walking the sheets in declaration order, so a handle is stable, flat, and
/// known at compile time.
///
typedef struct {
  char name[SCENE_NAME_MAX];
  int sheet;
  int cel;
} scene_sprite;

///
/// @brief Declarative description of a scene.
///
/// The scene is the single source of truth for the world: it fixes the entity
/// pool size, the sprite sheet to draw from, and the named globals the
/// behaviour program works with. The engine reads it, the behaviour compiler
/// reads the same file, and the compiled slot count is checked against the
/// loaded scene, so the two cannot drift apart silently.
///
typedef struct {
  char name[SCENE_NAME_MAX];
  int entity_count;
  uint8_t background;
  uint32_t seed;
  int global_count;
  scene_global global[SCENE_MAX_GLOBALS];
  int sheet_count;
  scene_sheet sheet[SCENE_MAX_SHEETS];
  int sprite_count;
  scene_sprite sprite[SCENE_MAX_SPRITES];
} scene;

///
/// @brief Parses @p path into @p this.
///
/// The file is line oriented: every non-blank, non-comment line is
/// "key value"; 'slot' takes "slot <name> <initial>". Unknown keys and values
/// out of range are errors, so a broken scene fails at load time rather than
/// mid-frame.
///
/// @return false and fills @p error with "line N: ..." when the file is invalid.
///
bool scene_load(scene *this, const char *path, char *error, size_t error_size);

///
/// @brief Number of slots the world state needs for @p this.
///
int scene_slot_count(const scene *this);

///
/// @brief Number of cels across every sheet @p this declares.
///
int scene_cel_count(const scene *this);

///
/// @brief Handle of the @p index'th named sprite: its cel's place in the flat
/// list every loaded sheet is appended to.
///
int scene_sprite_handle(const scene *this, int index);

///
/// @brief Index of the sprite named @p name, or -1.
///
int scene_find_sprite(const scene *this, const char *name);

///
/// @brief Slot index of the global named @p name, or -1.
///
int scene_slot_of(const scene *this, const char *name);

///
/// @brief Describes the scene in one line, for diagnostics.
///
void scene_describe(const scene *this, char *buffer, size_t buffer_size);

#endif
