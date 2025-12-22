#ifndef RECT_H_INCLUDED
#define RECT_H_INCLUDED

#include <common/size.h>

#include <stdint.h>

typedef struct rect_s {
  uint32_t x;
  uint32_t y;
  size size;
} rect;

#endif
