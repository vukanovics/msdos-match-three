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
#include <sys/nearptr.h>
#include <time.h>

#define BOARD_COLUMNS 10
#define BOARD_ROWS 10
#define TILE_TYPES MATCH3_TILE_COUNT
#define TILE_SIZE MATCH3_TILE_WIDTH
#define EMPTY (-1)

// Bytes per plane for one 320x240 page.
#define PAGE_SIZE ((SCREEN_WIDTH * SCREEN_HEIGHT) / 4)

#define CURSOR_COLOR MATCH3_CURSOR_COLOR
#define PICKED_COLOR MATCH3_PICKED_COLOR
#define BACKGROUND_COLOR MATCH3_BACKGROUND_COLOR

// Falling acceleration in pixels per second squared; lower is slower.
#define GRAVITY 1500.0f

// Seconds for two tiles to trade places.
#define SWAP_DURATION 0.18f

typedef enum phase_e {
  PHASE_IDLE,
  PHASE_SWAPPING,
  PHASE_FALLING
} phase;

struct game_s {
  int board[BOARD_ROWS][BOARD_COLUMNS];
  planarized_image *tiles[TILE_TYPES];

  int origin_x;
  int origin_y;
  int selected_row;
  int selected_column;
  int picked_row;
  int picked_column;

  size_t draw_base;
  int back_page;

  phase current_phase;
  float phase_time;

  int swap_a_row;
  int swap_a_column;
  int swap_b_row;
  int swap_b_column;

  int fall_board[BOARD_ROWS][BOARD_COLUMNS];
  int fall_start_row[BOARD_ROWS][BOARD_COLUMNS];
};

static uint8_t *video_memory(void) {
  return (uint8_t *)(0xa0000 + __djgpp_conventional_base);
}

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

static void clear_screen(game *g) {
  uint8_t *vram = video_memory();
  int width_per_plane = SCREEN_WIDTH / 4;

  for (int row = 0; row < SCREEN_HEIGHT; row++) {
    size_t offset = g->draw_base + (size_t)row * width_per_plane;
    for (int plane = 0; plane < 4; plane++) {
      set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));
      memset(vram + offset, BACKGROUND_COLOR, width_per_plane);
    }
  }
}

// Draws a tile with its top-left at (pixel_x, pixel_y), clipping vertically to
// the screen. `pixel_x` must be a multiple of 4.
static void draw_tile_at(game *g, int type, int pixel_x, int pixel_y) {
  int top = pixel_y;
  int skip = 0;
  if (top < 0) {
    skip = -top;
    top = 0;
  }

  int bottom = pixel_y + TILE_SIZE;
  if (bottom > SCREEN_HEIGHT) {
    bottom = SCREEN_HEIGHT;
  }

  int rows = bottom - top;
  if (rows <= 0) {
    return;
  }

  int width_per_plane = TILE_SIZE / 4;
  uint8_t *planes[4] = {
      planarized_image_get_plane_1_data(g->tiles[type]),
      planarized_image_get_plane_2_data(g->tiles[type]),
      planarized_image_get_plane_3_data(g->tiles[type]),
      planarized_image_get_plane_4_data(g->tiles[type]),
  };
  uint8_t *vram = video_memory();

  for (int row = 0; row < rows; row++) {
    size_t destination = g->draw_base +
                         (size_t)(top + row) * (SCREEN_WIDTH / 4) +
                         (pixel_x / 4);
    size_t source = (size_t)(skip + row) * width_per_plane;
    for (int plane = 0; plane < 4; plane++) {
      set_sc(&SC_MAP_MASK, (uint8_t)(1 << plane));
      memcpy(vram + destination, planes[plane] + source, width_per_plane);
    }
  }
}

static void draw_border(game *g, int row, int column, uint8_t color) {
  int x = g->origin_x + column * TILE_SIZE;
  int y = g->origin_y + row * TILE_SIZE;

  for (int i = 0; i < TILE_SIZE; i++) {
    write_pixel((uint16_t)(x + i), (uint16_t)y, color);
    write_pixel((uint16_t)(x + i), (uint16_t)(y + TILE_SIZE - 1), color);
    write_pixel((uint16_t)x, (uint16_t)(y + i), color);
    write_pixel((uint16_t)(x + TILE_SIZE - 1), (uint16_t)(y + i), color);
  }
}

static int tile_pixel_x(game *g, int column) {
  return g->origin_x + column * TILE_SIZE;
}

static int tile_pixel_y(game *g, int row) {
  return g->origin_y + row * TILE_SIZE;
}

static void draw_static_tiles(game *g) {
  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      int type = g->board[row][column];
      if (type != EMPTY) {
        draw_tile_at(g, type, tile_pixel_x(g, column),
                     tile_pixel_y(g, row));
      }
    }
  }
}

static void draw_swap(game *g) {
  float t = g->phase_time / SWAP_DURATION;
  if (t > 1.0f) {
    t = 1.0f;
  }
  t = t * t * (3.0f - 2.0f * t);

  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      int is_a = row == g->swap_a_row && column == g->swap_a_column;
      int is_b = row == g->swap_b_row && column == g->swap_b_column;
      if (!is_a && !is_b) {
        draw_tile_at(g, g->board[row][column], tile_pixel_x(g, column),
                     tile_pixel_y(g, row));
      }
    }
  }

  int ax = tile_pixel_x(g, g->swap_a_column);
  int ay = tile_pixel_y(g, g->swap_a_row);
  int bx = tile_pixel_x(g, g->swap_b_column);
  int by = tile_pixel_y(g, g->swap_b_row);

  rect a = {.x = (uint32_t)(ax + (int)((bx - ax) * t)),
            .y = (uint32_t)(ay + (int)((by - ay) * t)),
            .size = {.h = TILE_SIZE, .w = TILE_SIZE}};
  rect b = {.x = (uint32_t)(bx + (int)((ax - bx) * t)),
            .y = (uint32_t)(by + (int)((ay - by) * t)),
            .size = {.h = TILE_SIZE, .w = TILE_SIZE}};

  copy_to_framebuffer(g->tiles[g->board[g->swap_a_row][g->swap_a_column]], a);
  copy_to_framebuffer(g->tiles[g->board[g->swap_b_row][g->swap_b_column]], b);
}

static void draw_falling(game *g) {
  float fallen = 0.5f * GRAVITY * g->phase_time * g->phase_time;

  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      int start_row = g->fall_start_row[row][column];
      float distance = (float)(row - start_row) * TILE_SIZE;
      float travelled = fallen < distance ? fallen : distance;
      int y = g->origin_y + start_row * TILE_SIZE + (int)travelled;

      draw_tile_at(g, g->fall_board[row][column], tile_pixel_x(g, column), y);
    }
  }
}

static void begin_fall(game *g) {
  g->current_phase = PHASE_FALLING;
  g->phase_time = 0.0f;

  for (int column = 0; column < BOARD_COLUMNS; column++) {
    int write_row = BOARD_ROWS - 1;

    for (int row = BOARD_ROWS - 1; row >= 0; row--) {
      if (g->board[row][column] != EMPTY) {
        g->fall_board[write_row][column] = g->board[row][column];
        g->fall_start_row[write_row][column] = row;
        write_row--;
      }
    }

    int new_count = write_row + 1;
    for (int row = write_row; row >= 0; row--) {
      g->fall_board[row][column] = rand() % TILE_TYPES;
      g->fall_start_row[row][column] = row - new_count;
    }
  }
}

static void begin_resolution(game *g) {
  int marked[BOARD_ROWS][BOARD_COLUMNS];
  find_matches(g, marked);

  int any_marked = 0;
  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      if (marked[row][column]) {
        g->board[row][column] = EMPTY;
        any_marked = 1;
      }
    }
  }

  if (!any_marked) {
    g->current_phase = PHASE_IDLE;
    return;
  }

  begin_fall(g);
}

static bool fall_settled(game *g) {
  float fallen = 0.5f * GRAVITY * g->phase_time * g->phase_time;

  for (int row = 0; row < BOARD_ROWS; row++) {
    for (int column = 0; column < BOARD_COLUMNS; column++) {
      float distance = (float)(row - g->fall_start_row[row][column]) * TILE_SIZE;
      if (fallen < distance) {
        return false;
      }
    }
  }
  return true;
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

  this->current_phase = PHASE_IDLE;
  this->phase_time = 0.0f;

  // Draw to page 1 first; page 0 is shown until the first flip.
  this->back_page = 1;
  this->draw_base = PAGE_SIZE;

  set_gc(&GC_BIT_MASK, 0xff);

  fill_random(this);
  begin_resolution(this);

  return this;
}

void game_free(game *this) { free(this); }

void game_render(game *this) {
  set_draw_page(this->draw_base);
  clear_screen(this);

  switch (this->current_phase) {
  case PHASE_SWAPPING:
    draw_swap(this);
    break;
  case PHASE_FALLING:
    draw_falling(this);
    break;
  case PHASE_IDLE:
  default:
    draw_static_tiles(this);
    break;
  }

  if (this->picked_row >= 0) {
    draw_border(this, this->picked_row, this->picked_column, PICKED_COLOR);
  }
  draw_border(this, this->selected_row, this->selected_column, CURSOR_COLOR);

  // Show the page we just finished drawing, then draw to the other one next.
  set_display_start(this->draw_base);
  this->back_page ^= 1;
  this->draw_base = this->back_page ? PAGE_SIZE : 0;
}

void game_move_selection(game *this, int dx, int dy) {
  if (this->current_phase != PHASE_IDLE) {
    return;
  }

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
  if (this->current_phase != PHASE_IDLE) {
    return;
  }

  if (this->picked_row < 0) {
    this->picked_row = this->selected_row;
    this->picked_column = this->selected_column;
    return;
  }

  this->swap_a_row = this->picked_row;
  this->swap_a_column = this->picked_column;
  this->swap_b_row = this->selected_row;
  this->swap_b_column = this->selected_column;

  this->current_phase = PHASE_SWAPPING;
  this->phase_time = 0.0f;
}

bool game_update(game *this, float dt) {
  if (this->current_phase == PHASE_IDLE) {
    return false;
  }

  this->phase_time += dt;

  if (this->current_phase == PHASE_SWAPPING) {
    if (this->phase_time >= SWAP_DURATION) {
      int type = this->board[this->swap_a_row][this->swap_a_column];
      this->board[this->swap_a_row][this->swap_a_column] =
          this->board[this->swap_b_row][this->swap_b_column];
      this->board[this->swap_b_row][this->swap_b_column] = type;

      this->picked_row = -1;
      this->picked_column = -1;

      begin_resolution(this);
    }
    return true;
  }

  if (this->current_phase == PHASE_FALLING && fall_settled(this)) {
    memcpy(this->board, this->fall_board, sizeof(this->board));
    begin_resolution(this);
  }

  return true;
}

bool game_is_animating(game *this) {
  return this->current_phase != PHASE_IDLE;
}
