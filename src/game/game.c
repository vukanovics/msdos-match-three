#include "game/game.h"

#include "common/image.h"
#include "common/rect.h"
#include "common/size.h"
#include "match3_tiles.h"
#include "vga/planarized_image.h"
#include "vga/vga.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BOARD_COLUMNS 10
#define BOARD_ROWS 10
#define TILE_TYPES MATCH3_TILE_COUNT
#define TILE_SIZE MATCH3_TILE_WIDTH
#define EMPTY (-1)

#define CURSOR_COLOR MATCH3_CURSOR_COLOR
#define PICKED_COLOR MATCH3_PICKED_COLOR

struct game_s {
  int board[BOARD_ROWS][BOARD_COLUMNS];
  planarized_image *tiles[TILE_TYPES];
  int origin_x;
  int origin_y;
  int selected_row;
  int selected_column;
  int picked_row;
  int picked_column;
};

// Copies the generated 24x24 palette-indexed tile data into a planarized image.
static planarized_image *make_tile(int type) {
  size tile_size = {.h = MATCH3_TILE_HEIGHT, .w = MATCH3_TILE_WIDTH};
  size_t bytes = MATCH3_TILE_WIDTH * MATCH3_TILE_HEIGHT;

  uint8_t *data = malloc(bytes);
  memcpy(data, match3_tiles[type], bytes);

  return planarized_image_new(image_new(data, tile_size));
}

static void fill_random(game *this) {
  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      this->board[row][column] = rand() % TILE_TYPES;
    }
  }
}

static void find_matches(game *this, int marked[BOARD_ROWS][BOARD_COLUMNS]) {
  memset(marked, 0, sizeof(int) * BOARD_ROWS * BOARD_COLUMNS);

  for (int row = 0; row < BOARD_ROWS; row++) {
    int column = 0;
    while (column < BOARD_COLUMNS) {
      int type = this->board[row][column];
      int end = column;
      while (end + 1 < BOARD_COLUMNS && this->board[row][end + 1] == type) {
        end++;
      }
      if (type != EMPTY && end - column + 1 >= 3) {
        for (int c = column; c <= end; c++) {
          marked[row][c] = 1;
        }
      }
      column = end + 1;
    }
  }

  for (int column = 0; column < BOARD_COLUMNS; column++) {
    int row = 0;
    while (row < BOARD_ROWS) {
      int type = this->board[row][column];
      int end = row;
      while (end + 1 < BOARD_ROWS && this->board[end + 1][column] == type) {
        end++;
      }
      if (type != EMPTY && end - row + 1 >= 3) {
        for (int r = row; r <= end; r++) {
          marked[r][column] = 1;
        }
      }
      row = end + 1;
    }
  }
}

// Drops surviving tiles to the bottom of each column and fills the gaps at the
// top with new tiles.
static void collapse_and_refill(game *this) {
  for (int column = 0; column < BOARD_COLUMNS; column++) {
    int write_row = BOARD_ROWS - 1;

    for (int row = BOARD_ROWS - 1; row >= 0; row--) {
      if (this->board[row][column] != EMPTY) {
        this->board[write_row][column] = this->board[row][column];
        if (write_row != row) {
          this->board[row][column] = EMPTY;
        }
        write_row--;
      }
    }

    for (int row = write_row; row >= 0; row--) {
      this->board[row][column] = rand() % TILE_TYPES;
    }
  }
}

// Clears matches, then cascades until the board is stable.
static void resolve(game *this) {
  for (;;) {
    int marked[BOARD_ROWS][BOARD_COLUMNS];
    find_matches(this, marked);

    int any_marked = 0;
    for (int row = 0; row < BOARD_ROWS; row++) {
      for (int column = 0; column < BOARD_COLUMNS; column++) {
        if (marked[row][column]) {
          this->board[row][column] = EMPTY;
          any_marked = 1;
        }
      }
    }

    if (!any_marked) {
      return;
    }

    collapse_and_refill(this);
  }
}

game *game_new(void) {
  game *this = malloc(sizeof(game));

  srand((unsigned)time(NULL));
  set_palette((const uint8_t *)match3_palette, 256);

  for (int type = 0; type < TILE_TYPES; type++) {
    this->tiles[type] = make_tile(type);
  }

  int board_width = BOARD_COLUMNS * TILE_SIZE;
  int board_height = BOARD_ROWS * TILE_SIZE;
  this->origin_x = (SCREEN_WIDTH - board_width) / 2;
  this->origin_y = (SCREEN_HEIGHT - board_height) / 2;

  this->selected_row = BOARD_ROWS / 2;
  this->selected_column = BOARD_COLUMNS / 2;
  this->picked_row = -1;
  this->picked_column = -1;

  // Ensure writes reach every plane before drawing.
  set_gc(&GC_BIT_MASK, 0xff);

  fill_random(this);
  resolve(this);

  return this;
}

void game_free(game *this) { free(this); }

static void draw_tile(game *this, int row, int column) {
  int type = this->board[row][column];
  if (type == EMPTY) {
    return;
  }

  rect target = {.x = this->origin_x + column * TILE_SIZE,
                 .y = this->origin_y + row * TILE_SIZE,
                 .size = {.h = TILE_SIZE, .w = TILE_SIZE}};
  copy_to_framebuffer(this->tiles[type], target);
}

static void draw_border(game *this, int row, int column, uint8_t color) {
  int x = this->origin_x + column * TILE_SIZE;
  int y = this->origin_y + row * TILE_SIZE;

  for (int i = 0; i < TILE_SIZE; i++) {
    write_pixel((uint16_t)(x + i), (uint16_t)y, color);
    write_pixel((uint16_t)(x + i), (uint16_t)(y + TILE_SIZE - 1), color);
    write_pixel((uint16_t)x, (uint16_t)(y + i), color);
    write_pixel((uint16_t)(x + TILE_SIZE - 1), (uint16_t)(y + i), color);
  }
}

void game_render(game *this) {
  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      draw_tile(this, row, column);
    }
  }

  if (this->picked_row >= 0) {
    draw_border(this, this->picked_row, this->picked_column, PICKED_COLOR);
  }
  draw_border(this, this->selected_row, this->selected_column, CURSOR_COLOR);
}

void game_move_selection(game *this, int dx, int dy) {
  int row = this->selected_row + dy;
  int column = this->selected_column + dx;

  if (row >= 0 && row < BOARD_ROWS) {
    this->selected_row = row;
  }
  if (column >= 0 && column < BOARD_COLUMNS) {
    this->selected_column = column;
  }
}

void game_select(game *this) {
  if (this->picked_row < 0) {
    this->picked_row = this->selected_row;
    this->picked_column = this->selected_column;
    return;
  }

  int picked_type = this->board[this->picked_row][this->picked_column];
  this->board[this->picked_row][this->picked_column] =
      this->board[this->selected_row][this->selected_column];
  this->board[this->selected_row][this->selected_column] = picked_type;

  this->picked_row = -1;
  this->picked_column = -1;

  resolve(this);
}
