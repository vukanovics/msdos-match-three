#ifndef MATCH3_SHEET_H_INCLUDED
#define MATCH3_SHEET_H_INCLUDED

#include "engine/sprite.h"

// The match-three sprite sheet: six tile cels from the generated sheet, plus a
// cursor frame and a picked frame drawn as hollow boxes. The engine only ever
// sees the resulting sheet; nothing about these cels is compiled into it.
extern const sheet_source match3_sheets;

#endif
