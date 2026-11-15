#include "log.h"
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
  test_log_open("tests.log");
  test_log("Running VGA tests:\n");

  test_log("crtc_set_get_mono: %s\n",
           crtc_set_get_mono() ? success() : failed());
  test_log("crtc_set_get_color: %s\n",
           crtc_set_get_color() ? success() : failed());

  test_log("ac_set_get_mono: %s\n", ac_set_get_mono() ? success() : failed());
  test_log("ac_set_get_color: %s\n",
           ac_set_get_color() ? success() : failed());

  test_log("sc_set_get: %s\n", sc_set_get() ? success() : failed());
  test_log("sc_disable_chain4: %s\n",
           sc_disable_chain4() ? success() : failed());

  test_log("gc_set_get: %s\n", gc_set_get() ? success() : failed());

  test_log("copy_to_framebuffer_x_offset_0: %s\n",
           copy_to_framebuffer_x_offset_0() ? success() : failed());
  test_log("copy_to_framebuffer_x_offset_1: %s\n",
           copy_to_framebuffer_x_offset_1() ? success() : failed());
  test_log("copy_to_framebuffer_x_offset_2: %s\n",
           copy_to_framebuffer_x_offset_2() ? success() : failed());
  test_log("copy_to_framebuffer_x_offset_3: %s\n",
           copy_to_framebuffer_x_offset_3() ? success() : failed());

  test_log("framebuffer_copy_x_offset_0: %s\n",
           framebuffer_copy_x_offset_0() ? success() : failed());
  test_log("framebuffer_copy_x_offset_1: %s\n",
           framebuffer_copy_x_offset_1() ? success() : failed());
  test_log("framebuffer_copy_x_offset_2: %s\n",
           framebuffer_copy_x_offset_2() ? success() : failed());
  test_log("framebuffer_copy_x_offset_3: %s\n",
           framebuffer_copy_x_offset_3() ? success() : failed());
  test_log("framebuffer_copy_y_offset: %s\n",
           framebuffer_copy_y_offset() ? success() : failed());

  if (any_failed != 0) {
    test_log("###############################################################\n");
    test_log("######################SOME TESTS FAILED!#######################\n");
    test_log("###############################################################\n");
    test_log_close();
    return 1;
  }

  test_log_close();
  return 0;
}
