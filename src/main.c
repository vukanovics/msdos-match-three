#include "game/game.h"
#include "vga/vga.h"

#include <conio.h>
#include <sys/nearptr.h>

#define KEY_UP 0x48
#define KEY_DOWN 0x50
#define KEY_LEFT 0x4b
#define KEY_RIGHT 0x4d
#define KEY_ENTER '\r'
#define KEY_ESCAPE 0x1b

int main(int argc, char **argv) {
  switch_to_mode_x();
  __djgpp_nearptr_enable();

  game *current_game = game_new();
  game_render(current_game);

  int running = 1;
  while (running) {
    int key = getch();

    if (key == 0) {
      int extended = getch();
      switch (extended) {
      case KEY_UP:
        game_move_selection(current_game, 0, -1);
        break;
      case KEY_DOWN:
        game_move_selection(current_game, 0, 1);
        break;
      case KEY_LEFT:
        game_move_selection(current_game, -1, 0);
        break;
      case KEY_RIGHT:
        game_move_selection(current_game, 1, 0);
        break;
      }
      game_render(current_game);
    } else if (key == KEY_ENTER) {
      game_select(current_game);
      game_render(current_game);
    } else if (key == KEY_ESCAPE) {
      running = 0;
    }
  }

  game_free(current_game);
  __djgpp_nearptr_disable();
  switch_to_mode_3();
  return 0;
}
