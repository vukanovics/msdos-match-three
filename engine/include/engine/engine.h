#ifndef ENGINE_ENGINE_H_INCLUDED
#define ENGINE_ENGINE_H_INCLUDED

#include "engine/demo.h"
#include "engine/sprite.h"

///
/// @brief Runs a game described by @p scene_path and @p behaviour_path.
///
/// The engine owns the clock, the display and the frame loop; the game supplies
/// only data - a scene file, a compiled behaviour program, and the sprite
/// sheets the scene can name - so a different game is a different set of files,
/// not a different engine.
///
/// @p sheets is a NULL terminated list of providers; see sheet_source.
///
/// @p mode decides whether the run is played normally, recorded to
/// @p demo_path, or replayed from it.
///
/// @return the process exit code: 0 on a clean quit, 1 when a file is invalid.
///
int engine_main(const char *scene_path, const char *behaviour_path,
                const sheet_source *const *sheets, demo_mode mode,
                const char *demo_path);

#endif
