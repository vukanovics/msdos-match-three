// The match-three game, as data: a scene file, a compiled behaviour program,
// and a sprite sheet. Everything else is the engine.

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "engine/engine.h"

#define SCENE_PATH "MATCH3.SCN"
#define BEHAVIOUR_PATH "MATCH3.BC"
#define DEMO_PATH "MATCH3.DEM"

// DOS may hand the arguments over in either case.
static int same(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return 0;
    }
    a++;
    b++;
  }
  return *a == *b;
}

static void usage(void) {
  printf("match3 [scene] [behaviour] [/rec | /play]\n"
         "  scene      defaults to %s\n"
         "  behaviour  defaults to %s, compiled from the same scene\n"
         "  /rec       record keys and checksums to %s\n"
         "  /play      replay %s and check every frame\n",
         SCENE_PATH, BEHAVIOUR_PATH, DEMO_PATH, DEMO_PATH);
}

int main(int argc, char **argv) {
  demo_mode mode = DEMO_OFF;
  const char *scene_path = SCENE_PATH;
  const char *behaviour_path = BEHAVIOUR_PATH;
  const char *positional[2] = {NULL, NULL};
  int positional_count = 0;

  for (int i = 1; i < argc; i++) {
    if (same(argv[i], "/rec") || same(argv[i], "-rec")) {
      mode = DEMO_RECORD;
    } else if (same(argv[i], "/play") || same(argv[i], "-play")) {
      mode = DEMO_PLAY;
    } else if (positional_count < 2) {
      positional[positional_count++] = argv[i];
    } else {
      usage();
      return 1;
    }
  }

  // The scene and the program are data, so a different pair is a different game
  // without a rebuild: match3 level2.scn level2.bc.
  if (positional_count > 0) {
    scene_path = positional[0];
  }
  if (positional_count > 1) {
    behaviour_path = positional[1];
  }

  return engine_main(scene_path, behaviour_path, mode, DEMO_PATH);
}
