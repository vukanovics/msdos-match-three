#include "engine/scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LENGTH 128

static void set_defaults(scene *this) {
  memset(this, 0, sizeof(*this));
  strcpy(this->name, "untitled");
  this->columns = 10;
  this->rows = 10;
  this->tile_size = 24;
  this->background = 253;
  this->cursor = 254;
  this->picked = 255;
  this->sprite_count = 6;
  this->sprite_width = 24;
  this->sprite_height = 24;
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

static bool assign(scene *this, const char *key, const char *value, char *error,
                   size_t error_size, int line) {
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

  if (!strcmp(key, "board.columns")) {
    this->columns = number;
  } else if (!strcmp(key, "board.rows")) {
    this->rows = number;
  } else if (!strcmp(key, "board.tile")) {
    this->tile_size = number;
  } else if (!strcmp(key, "colors.background")) {
    this->background = (uint8_t)number;
  } else if (!strcmp(key, "colors.cursor")) {
    this->cursor = (uint8_t)number;
  } else if (!strcmp(key, "colors.picked")) {
    this->picked = (uint8_t)number;
  } else if (!strcmp(key, "sprites.tiles.count")) {
    this->sprite_count = number;
  } else if (!strcmp(key, "sprites.tiles.width")) {
    this->sprite_width = number;
  } else if (!strcmp(key, "sprites.tiles.height")) {
    this->sprite_height = number;
  } else if (!strcmp(key, "world.seed")) {
    this->seed = (uint32_t)number;
  } else {
    snprintf(error, error_size, "line %d: unknown key '%s'", line, key);
    return false;
  }
  return true;
}

static bool parse_line(scene *this, char *line, char *error, size_t error_size,
                       int line_number) {
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

  return assign(this, key, value, error, error_size, line_number);
}

static bool validate(const scene *this, char *error, size_t error_size) {
  if (this->columns < 3 || this->columns > SCENE_MAX_COLUMNS) {
    snprintf(error, error_size, "board.columns must be 3..%d",
             SCENE_MAX_COLUMNS);
    return false;
  }
  if (this->rows < 3 || this->rows > SCENE_MAX_ROWS) {
    snprintf(error, error_size, "board.rows must be 3..%d", SCENE_MAX_ROWS);
    return false;
  }
  if (this->tile_size < 4 || this->tile_size > 64) {
    snprintf(error, error_size, "board.tile must be 4..64");
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

  while (fgets(line, sizeof(line), file)) {
    number++;
    if (!parse_line(this, line, error, error_size, number)) {
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
    ok = validate(this, error, error_size);
  }
  return ok;
}

void scene_describe(const scene *this, char *buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size,
           "scene %s %dx%d tile %d sprites %d@%dx%d seed %lu",
           this->name, this->columns, this->rows, this->tile_size,
           this->sprite_count, this->sprite_width, this->sprite_height,
           (unsigned long)this->seed);
}
