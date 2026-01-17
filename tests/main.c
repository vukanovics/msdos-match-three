#include "vga/test_vga.h"
#include <conio.h>
#include <stdio.h>

static char *SUCCESS = "success";
static char *FAILED = "### FAILED! ###";

static int any_failed = 0;

char *success() { return SUCCESS; }
char *failed() {
  any_failed = 1;
  return FAILED;
}

int main(int argc, char **argv) {
  printf("Running VGA tests:\n");

  printf("crtc_set_get_mono: %s\n", crtc_set_get_mono() ? success() : failed());
  printf("crtc_set_get_color: %s\n",
         crtc_set_get_color() ? success() : failed());

  printf("ac_set_get_mono: %s\n", ac_set_get_mono() ? success() : failed());
  printf("ac_set_get_color: %s\n", ac_set_get_color() ? success() : failed());

  printf("sc_set_get: %s\n", sc_set_get() ? success() : failed());
  printf("sc_disable_chain4: %s\n", sc_disable_chain4() ? success() : failed());

  printf("gc_set_get: %s\n", gc_set_get() ? success() : failed());

  printf("copy_to_framebuffer_x_offset_0: %s\n",
         copy_to_framebuffer_x_offset_0() ? success() : failed());
  printf("copy_to_framebuffer_x_offset_1: %s\n",
         copy_to_framebuffer_x_offset_1() ? success() : failed());
  printf("copy_to_framebuffer_x_offset_2: %s\n",
         copy_to_framebuffer_x_offset_2() ? success() : failed());
  printf("copy_to_framebuffer_x_offset_3: %s\n",
         copy_to_framebuffer_x_offset_3() ? success() : failed());

  if (any_failed != 0) {
    printf("###############################################################\n");
    printf("######################SOME TESTS FAILED!#######################\n");
    printf("###############################################################\n");
    return 1;
  }
  return 0;
}
