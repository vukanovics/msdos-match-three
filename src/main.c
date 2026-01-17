#include "vga/vga.h"
#include <stdio.h>

int main(int argc, char **argv) {
  switch_to_mode_x();
  getchar();
  switch_to_mode_3();
  printf("Hello world!\n");
  return 0;
}
