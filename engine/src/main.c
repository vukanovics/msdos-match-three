// The match-three game, as data: a scene file, a compiled behaviour program,
// and the sprite sheet provider. Everything else is the engine.

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "engine/engine.h"
#include "match3_sheet.h"

#define SCENE_PATH "MATCH3.SCN"
#define BEHAVIOUR_PATH "MATCH3.BC"
#define DEMO_PATH "MATCH3.DEM"

static const sheet_source *const sheets[] = {&match3_sheets, NULL};

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
  printf("match3            play\n"
         "match3 /rec       play, recording keys and checksums to %s\n"
         "match3 /play      replay %s and check every frame\n",
         DEMO_PATH, DEMO_PATH);
}

int main(int argc, char **argv) {
  demo_mode mode = DEMO_OFF;

  for (int i = 1; i < argc; i++) {
    if (same(argv[i], "/rec") || same(argv[i], "-rec")) {
      mode = DEMO_RECORD;
    } else if (same(argv[i], "/play") || same(argv[i], "-play")) {
      mode = DEMO_PLAY;
    } else {
      usage();
      return 1;
    }
  }

  return engine_main(SCENE_PATH, BEHAVIOUR_PATH, sheets, mode, DEMO_PATH);
}
