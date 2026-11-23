#include "engine/scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LENGTH 128

static void set_defaults(scene *this) {
  memset(this, 0, sizeof(*this));
  strcpy(this->name, "untitled");
  this->background = 0;
  this->seed = 1;
  this->animation_ticks = 1;
}

static bool parse_int(const char *value, int *result) {
  char *end;
  long number = strtol(value, &end, 10);
  if (!*value || *end || number < -2147483647L || number > 2147483647L) {
    return false;
  }
  *result = (int)number;
  return true;
}

// Splits "name rest" into a name and a remainder, in place. Returns NULL when
// there is no remainder, which is what a 'slot' line without an initial value
// looks like.
static char *split_word(char *text) {
  char *at = text;
  while (*at && *at != ' ' && *at != '\t') {
    at++;
  }
  if (!*at) {
    return NULL;
  }
  *at++ = '\0';
  while (*at == ' ' || *at == '\t') {
    at++;
  }
  return *at ? at : NULL;
}

static bool append_global(scene *this, const char *name, int32_t initial,
                          char *error, size_t error_size, int line) {
  if (*name == '@' || *name == '!') {
    snprintf(error, error_size, "line %d: slot name '%s' must not start with @ "
                                "or !",
             line, name);
    return false;
  }
  if (strlen(name) >= SCENE_NAME_MAX) {
    snprintf(error, error_size, "line %d: slot name '%s' is too long", line,
             name);
    return false;
  }
  if (this->global_count >= SCENE_MAX_GLOBALS) {
    snprintf(error, error_size, "line %d: more than %d slots", line,
             SCENE_MAX_GLOBALS);
    return false;
  }
  for (int i = 0; i < this->global_count; i++) {
    if (!strcmp(this->global[i].name, name)) {
      snprintf(error, error_size, "line %d: duplicate slot '%s'", line, name);
      return false;
    }
  }

  scene_global *global = &this->global[this->global_count++];
  strncpy(global->name, name, SCENE_NAME_MAX - 1);
  global->name[SCENE_NAME_MAX - 1] = '\0';
  global->initial = initial;
  return true;
}

static bool add_global(scene *this, char *value, char *error,
                       size_t error_size, int line) {
  char *name = value;
  char *initial_text = split_word(value);
  if (!initial_text) {
    snprintf(error, error_size, "line %d: 'slot' needs 'slot <name> <value>'",
             line);
    return false;
  }

  int initial;
  if (!parse_int(initial_text, &initial)) {
    snprintf(error, error_size, "line %d: slot '%s' needs a whole number, got "
                                "'%s'",
             line, name, initial_text);
    return false;
  }

  return append_global(this, name, initial, error, error_size, line);
}

// 'array <name> <count> <value>' declares count slots called name0..name(count-1)
// holding the same value. The behaviour program keeps its per-object state in
// these, so the engine never has to know about it.
static bool add_array(scene *this, char *value, char *error, size_t error_size,
                      int line) {
  char *name = value;
  char *count_text = split_word(value);
  char *initial_text = count_text ? split_word(count_text) : NULL;
  if (!initial_text) {
    snprintf(error, error_size,
             "line %d: 'array' needs 'array <name> <count> <value>'", line);
    return false;
  }

  int count;
  int initial;
  if (!parse_int(count_text, &count) || count < 1 ||
      count > SCENE_MAX_GLOBALS) {
    snprintf(error, error_size,
             "line %d: array '%s' needs a count of 1..%d", line, name,
             SCENE_MAX_GLOBALS);
    return false;
  }
  if (!parse_int(initial_text, &initial)) {
    snprintf(error, error_size, "line %d: array '%s' needs a whole number, got "
                                "'%s'",
             line, name, initial_text);
    return false;
  }

  for (int i = 0; i < count; i++) {
    char element[SCENE_NAME_MAX + 12];
    snprintf(element, sizeof(element), "%s%d", name, i);
    if (!append_global(this, element, initial, error, error_size, line)) {
      return false;
    }
  }
  return true;
}

// 'sheet <name> <file> <cels>' and 'sprite <name> <sheet> <cel>' both carry more
// than one value, so they split their own.
//
// A sprite may only name a sheet declared above it. That is a small restriction
// on the author in exchange for an error on the right line, and for not having
// to keep unresolved names around.
static bool add_sheet(scene *this, char *value, char *error, size_t error_size,
                      int line) {
  char *name = value;
  char *path = split_word(value);
  char *count_text = path ? split_word(path) : NULL;
  if (!count_text) {
    snprintf(error, error_size,
             "line %d: 'sheet' needs 'sheet <name> <file> <cels>'", line);
    return false;
  }
  if (!*name || strlen(name) >= SCENE_NAME_MAX ||
      strlen(path) >= SCENE_NAME_MAX) {
    snprintf(error, error_size, "line %d: sheet names must be 1..%d chars",
             line, SCENE_NAME_MAX - 1);
    return false;
  }
  if (this->sheet_count >= SCENE_MAX_SHEETS) {
    snprintf(error, error_size, "line %d: more than %d sheets", line,
             SCENE_MAX_SHEETS);
    return false;
  }
  for (int i = 0; i < this->sheet_count; i++) {
    if (!strcmp(this->sheet[i].name, name)) {
      snprintf(error, error_size, "line %d: duplicate sheet '%s'", line, name);
      return false;
    }
  }

  int count;
  if (!parse_int(count_text, &count) || count < 1 || count > SCENE_MAX_CELS) {
    snprintf(error, error_size, "line %d: sheet '%s' needs 1..%d cels", line,
             name, SCENE_MAX_CELS);
    return false;
  }

  scene_sheet *sheet = &this->sheet[this->sheet_count++];
  strncpy(sheet->name, name, SCENE_NAME_MAX - 1);
  sheet->name[SCENE_NAME_MAX - 1] = '\0';
  strncpy(sheet->path, path, SCENE_NAME_MAX - 1);
  sheet->path[SCENE_NAME_MAX - 1] = '\0';
  sheet->cel_count = count;
  return true;
}

static bool add_sprite(scene *this, char *value, bool animated, char *error,
                       size_t error_size, int line) {
  const char *keyword = animated ? "animatedsprite" : "sprite";
  char *name = value;
  char *sheet_name = split_word(value);
  char *cel_text = sheet_name ? split_word(sheet_name) : NULL;
  char *rest = cel_text ? split_word(cel_text) : NULL;
  if (!cel_text) {
    snprintf(error, error_size,
             "line %d: '%s' needs '%s <name> <sheet> <cel>%s'%s", line, keyword,
             keyword, animated ? " <frames>" : "",
             animated ? " [loop] [resident]" : "");
    return false;
  }
  if (*name == '$') {
    snprintf(error, error_size,
             "line %d: sprite name '%s' must not start with $, which is how "
             "the behaviour program refers to it",
             line, name);
    return false;
  }
  if (!*name || strlen(name) >= SCENE_NAME_MAX) {
    snprintf(error, error_size, "line %d: sprite names must be 1..%d chars",
             line, SCENE_NAME_MAX - 1);
    return false;
  }
  if (this->sprite_count >= SCENE_MAX_SPRITES) {
    snprintf(error, error_size, "line %d: more than %d sprites", line,
             SCENE_MAX_SPRITES);
    return false;
  }
  for (int i = 0; i < this->sprite_count; i++) {
    if (!strcmp(this->sprite[i].name, name)) {
      snprintf(error, error_size, "line %d: duplicate sprite '%s'", line,
               name);
      return false;
    }
  }

  int sheet = -1;
  for (int i = 0; i < this->sheet_count; i++) {
    if (!strcmp(this->sheet[i].name, sheet_name)) {
      sheet = i;
      break;
    }
  }
  if (sheet < 0) {
    snprintf(error, error_size,
             "line %d: unknown sheet '%s' (declare sheets before the sprites "
             "that use them)",
             line, sheet_name);
    return false;
  }

  int cel;
  if (!parse_int(cel_text, &cel) || cel < 0 ||
      cel >= this->sheet[sheet].cel_count) {
    snprintf(error, error_size, "line %d: sheet '%s' has 0..%d cels, not '%s'",
             line, sheet_name, this->sheet[sheet].cel_count - 1, cel_text);
    return false;
  }

  int frames = 1;
  bool loop = false;
  bool resident = false;
  if (animated) {
    if (!rest) {
      snprintf(error, error_size, "line %d: '%s' needs a frame count", line,
               keyword);
      return false;
    }
    char *flags = split_word(rest);
    if (!parse_int(rest, &frames) || frames < 1) {
      snprintf(error, error_size,
               "line %d: sprite '%s' needs a frame count of 1 or more", line,
               name);
      return false;
    }
    while (flags && *flags) {
      char *flag = flags;
      flags = split_word(flags);
      if (!strcmp(flag, "loop")) {
        loop = true;
      } else if (!strcmp(flag, "resident")) {
        resident = true;
      } else {
        snprintf(error, error_size,
                 "line %d: sprite '%s' takes 'loop' or 'resident' after its "
                 "frame count, not '%s'",
                 line, name, flag);
        return false;
      }
    }
  } else if (rest) {
    snprintf(error, error_size,
             "line %d: '%s' is one cel; use 'animatedsprite' for a run", line,
             name);
    return false;
  }
  if (cel + frames > this->sheet[sheet].cel_count) {
    snprintf(error, error_size,
             "line %d: sprite '%s' runs to cel %d, past the %d cels of sheet "
             "'%s'",
             line, name, cel + frames, this->sheet[sheet].cel_count, sheet_name);
    return false;
  }

  scene_sprite *sprite = &this->sprite[this->sprite_count++];
  strncpy(sprite->name, name, SCENE_NAME_MAX - 1);
  sprite->name[SCENE_NAME_MAX - 1] = '\0';
  sprite->sheet = sheet;
  sprite->cel = cel;
  sprite->frames = frames;
  sprite->loop = loop;
  sprite->resident = resident;
  return true;
}

static bool assign(scene *this, const char *key, char *value, char *error,
                   size_t error_size, int line, bool *seen_entities) {
  if (!strcmp(key, "slot")) {
    return add_global(this, value, error, error_size, line);
  }
  if (!strcmp(key, "array")) {
    return add_array(this, value, error, error_size, line);
  }
  if (!strcmp(key, "sheet")) {
    return add_sheet(this, value, error, error_size, line);
  }
  if (!strcmp(key, "sprite")) {
    return add_sprite(this, value, false, error, error_size, line);
  }
  if (!strcmp(key, "animatedsprite")) {
    return add_sprite(this, value, true, error, error_size, line);
  }

  if (!strcmp(key, "scene.name")) {
    strncpy(this->name, value, SCENE_NAME_MAX - 1);
    this->name[SCENE_NAME_MAX - 1] = '\0';
    return true;
  }

  int number;
  if (!parse_int(value, &number)) {
    snprintf(error, error_size, "line %d: '%s' needs a whole number, got '%s'",
             line, key, value);
    return false;
  }

  if (!strcmp(key, "animation.ticks")) {
    this->animation_ticks = number;
  } else if (!strcmp(key, "entities.count")) {
    this->entity_count = number;
    *seen_entities = true;
  } else if (!strcmp(key, "color.background")) {
    this->background = (uint8_t)number;
  } else if (!strcmp(key, "world.seed")) {
    this->seed = (uint32_t)number;
  } else {
    snprintf(error, error_size, "line %d: unknown key '%s'", line, key);
    return false;
  }
  return true;
}

// 'array' takes "array <name> <count> <value>", so its value has two parts and
// the generic 'key value' split is done here instead.
static bool parse_line(scene *this, char *line, char *error, size_t error_size,
                       int line_number, bool *seen_entities) {
  char *comment = strchr(line, '#');
  if (comment) {
    *comment = '\0';
  }

  // Trim both ends. DOS text files end lines with CR LF, and a line that is
  // blank or only a comment is simply skipped.
  char *key = line;
  while (*key == ' ' || *key == '\t' || *key == '\r' || *key == '\n') {
    key++;
  }
  char *trim = key + strlen(key);
  while (trim > key && (trim[-1] == ' ' || trim[-1] == '\t' ||
                        trim[-1] == '\r' || trim[-1] == '\n')) {
    *--trim = '\0';
  }
  if (!*key) {
    return true;
  }

  char *value = key;
  while (*value && *value != ' ' && *value != '\t') {
    value++;
  }
  if (!*value) {
    snprintf(error, error_size, "line %d: expected 'key value'", line_number);
    return false;
  }
  *value++ = '\0';
  while (*value == ' ' || *value == '\t') {
    value++;
  }
  if (!*value) {
    snprintf(error, error_size, "line %d: '%s' needs a value", line_number, key);
    return false;
  }

  return assign(this, key, value, error, error_size, line_number,
                seen_entities);
}

static bool validate(const scene *this, bool seen_entities, char *error,
                     size_t error_size) {
  if (!seen_entities) {
    snprintf(error, error_size, "missing 'entities.count'");
    return false;
  }
  if (this->entity_count < 0 || this->entity_count > SCENE_MAX_ENTITIES) {
    snprintf(error, error_size, "entities.count must be 0..%d",
             SCENE_MAX_ENTITIES);
    return false;
  }
  if (this->sheet_count < 1) {
    snprintf(error, error_size, "the scene names no sprite sheet");
    return false;
  }
  int cels = scene_cel_count(this);
  if (this->animation_ticks < 1) {
    snprintf(error, error_size, "animation.ticks must be 1 or more");
    return false;
  }
  if (cels > SCENE_MAX_CELS) {
    snprintf(error, error_size,
             "the sheets hold %d cels, more than the %d the engine keeps",
             cels, SCENE_MAX_CELS);
    return false;
  }
  return true;
}

bool scene_load(scene *this, const char *path, char *error, size_t error_size) {
  set_defaults(this);

  FILE *file = fopen(path, "r");
  if (!file) {
    snprintf(error, error_size, "cannot open %s", path);
    return false;
  }

  char line[LINE_MAX_LENGTH];
  int number = 0;
  bool ok = true;
  bool seen_entities = false;

  while (fgets(line, sizeof(line), file)) {
    number++;
    if (!parse_line(this, line, error, error_size, number, &seen_entities)) {
      // Prefix the line number only when the error has none yet.
      if (strncmp(error, "line ", 5) != 0) {
        char detail[96];
        snprintf(detail, sizeof(detail), "%s", error);
        snprintf(error, error_size, "line %d: %s", number, detail);
      }
      ok = false;
      break;
    }
  }

  fclose(file);

  if (ok) {
    ok = validate(this, seen_entities, error, error_size);
  }
  return ok;
}

int scene_slot_count(const scene *this) {
  return this->entity_count * ENTITY_STRIDE + this->global_count;
}

int scene_cel_count(const scene *this) {
  int total = 0;
  for (int i = 0; i < this->sheet_count; i++) {
    total += this->sheet[i].cel_count;
  }
  return total;
}

int scene_sprite_handle(const scene *this, int index) {
  const scene_sprite *sprite = &this->sprite[index];
  int base = 0;
  for (int i = 0; i < sprite->sheet; i++) {
    base += this->sheet[i].cel_count;
  }
  return base + sprite->cel;
}

int scene_find_sprite(const scene *this, const char *name) {
  for (int i = 0; i < this->sprite_count; i++) {
    if (!strcmp(this->sprite[i].name, name)) {
      return i;
    }
  }
  return -1;
}

int scene_slot_of(const scene *this, const char *name) {
  for (int i = 0; i < this->global_count; i++) {
    if (!strcmp(this->global[i].name, name)) {
      return this->entity_count * ENTITY_STRIDE + i;
    }
  }
  return -1;
}

void scene_describe(const scene *this, char *buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size,
           "scene %s entities %d slots %d sheets %d sprites %d cels %d "
           "background %d seed %lu",
           this->name, this->entity_count, scene_slot_count(this),
           this->sheet_count, this->sprite_count, scene_cel_count(this),
           (int)this->background, (unsigned long)this->seed);
}
