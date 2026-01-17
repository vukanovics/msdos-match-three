#ifndef TEST_VGA_INCLUDED
#define TEST_VGA_INCLUDED

int crtc_set_get_mono();
int crtc_set_get_color();

int ac_set_get_mono();
int ac_set_get_color();

int sc_set_get();
int sc_disable_chain4();

int gc_set_get();

int copy_to_framebuffer_x_offset_0();
int copy_to_framebuffer_x_offset_1();
int copy_to_framebuffer_x_offset_2();
int copy_to_framebuffer_x_offset_3();

#endif
