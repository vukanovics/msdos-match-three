// The match-three game, as data: a scene file, a compiled behaviour program,
// and the sprite sheet provider. Everything else is the engine.

#include "engine/engine.h"
#include "match3_sheet.h"

#define SCENE_PATH "MATCH3.SCN"
#define BEHAVIOUR_PATH "MATCH3.BC"

static const sheet_source *const sheets[] = {&match3_sheets, NULL};

int main(void) {
  return engine_main(SCENE_PATH, BEHAVIOUR_PATH, sheets);
}
