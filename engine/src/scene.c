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

static bool add_global(scene *this, char *value, char *error,
                       size_t error_size, int line) {
  char *name = value;
  char *initial_text = split_word(value);
  if (!initial_text) {
    snprintf(error, error_size, "line %d: 'slot' needs 'slot <name> <value>'",
             line);
    return false;
  }

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

  int initial;
  if (!parse_int(initial_text, &initial)) {
    snprintf(error, error_size, "line %d: slot '%s' needs a whole number, got "
                                "'%s'",
             line, name, initial_text);
    return false;
  }

  scene_global *global = &this->global[this->global_count++];
  strncpy(global->name, name, SCENE_NAME_MAX - 1);
  global->name[SCENE_NAME_MAX - 1] = '\0';
  global->initial = initial;
  return true;
}

static bool assign(scene *this, const char *key, char *value, char *error,
                   size_t error_size, int line, bool *seen_entities) {
  if (!strcmp(key, "slot")) {
    return add_global(this, value, error, error_size, line);
  }

  if (!strcmp(key, "scene.name")) {
    strncpy(this->name, value, SCENE_NAME_MAX - 1);
    this->name[SCENE_NAME_MAX - 1] = '\0';
    return true;
  }
  if (!strcmp(key, "sheet")) {
    strncpy(this->sheet, value, SCENE_NAME_MAX - 1);
    this->sheet[SCENE_NAME_MAX - 1] = '\0';
    return true;
  }

  int number;
  if (!parse_int(value, &number)) {
    snprintf(error, error_size, "line %d: '%s' needs a whole number, got '%s'",
             line, key, value);
    return false;
  }

  if (!strcmp(key, "entities.count")) {
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
  if (!this->sheet[0]) {
    snprintf(error, error_size, "missing 'sheet'");
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

int scene_slot_of(const scene *this, const char *name) {
  for (int i = 0; i < this->global_count; i++) {
    if (!strcmp(this->global[i].name, name)) {
      return this->entity_count * ENTITY_STRIDE + i;
    }
  }
  return -1;
}

void scene_describe(const scene *this, char *buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "scene %s sheet %s entities %d slots %d seed %lu",
           this->name, this->sheet, this->entity_count, scene_slot_count(this),
           (unsigned long)this->seed);
}
