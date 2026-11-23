#!/usr/bin/env python3
"""Convert the match-three sprite sheet into the engine's .spr format.

The DJGPP target has no PNG library, so the sheet is converted on the host into
a small binary the engine can read at load time, rather than being compiled into
the executable.

Format (little endian):

    0    "SPR1"
    4    u16 width           cel width in pixels
    6    u16 height          cel height in pixels
    8    u16 count           number of cels
    10   u8  palette[768]    256 RGB triples, as set_palette expects them
    778  u8  transparent[n]  per cel: the index to treat as transparent
    778+n u8 pixels[n][w*h]  chunky palette indices, row major per cel

The palette is stored in the same 8-bit form the rest of the project uses;
set_palette scales it down to the six bits the DAC actually takes.

Usage: png_to_spr.py <sheet.png> <output.spr>
"""

import struct
import sys

from PIL import Image

TILE_WIDTH = 24
TILE_HEIGHT = 24
TILE_COUNT = 6

# Reserve the last three palette entries for the game's UI.
COLOR_COUNT = 253
BACKGROUND_INDEX = 253
CURSOR_INDEX = 254
PICKED_INDEX = 255

# The tiles are fully painted, so they use a key colour none of them contains.
# The UI boxes are two hollow rectangles, so their interior is the key.
TILE_TRANSPARENT = 255
BOX_TRANSPARENT = 0

BOX_THICKNESS = 2

MAGIC = b"SPR1"
HEADER_SIZE = 10
PALETTE_SIZE = 256 * 3


def draw_box(width, height, colour, thickness=BOX_THICKNESS):
    """A hollow rectangle: the border is opaque, the interior is the key."""
    cel = bytearray([BOX_TRANSPARENT]) * (width * height)
    for t in range(thickness):
        for x in range(t, width - t):
            cel[t * width + x] = colour
            cel[(height - 1 - t) * width + x] = colour
        for y in range(t, height - t):
            cel[y * width + t] = colour
            cel[y * width + width - 1 - t] = colour
    return cel


def main(sprite_path, output_path):
    sheet = Image.open(sprite_path).convert("RGBA")

    # Composite the transparency onto black, then reduce to a shared palette.
    background = Image.new("RGBA", sheet.size, (0, 0, 0, 255))
    sheet = Image.alpha_composite(background, sheet).convert("RGB")
    sheet = sheet.quantize(colors=COLOR_COUNT, method=Image.Quantize.MEDIANCUT)

    palette = sheet.getpalette()[: COLOR_COUNT * 3]
    palette += [0] * (PALETTE_SIZE - len(palette))
    palette[BACKGROUND_INDEX * 3:BACKGROUND_INDEX * 3 + 3] = [0, 0, 0]
    palette[CURSOR_INDEX * 3:CURSOR_INDEX * 3 + 3] = [255, 255, 255]
    palette[PICKED_INDEX * 3:PICKED_INDEX * 3 + 3] = [255, 0, 0]

    pixels = sheet.tobytes()
    stride = sheet.width
    if sheet.height != TILE_HEIGHT or sheet.width != TILE_WIDTH * TILE_COUNT:
        raise SystemExit(
            "expected a %dx%d sheet, got %dx%d"
            % (TILE_WIDTH * TILE_COUNT, TILE_HEIGHT, sheet.width, sheet.height)
        )

    cels = []
    for tile in range(TILE_COUNT):
        cel = bytearray(TILE_WIDTH * TILE_HEIGHT)
        for y in range(TILE_HEIGHT):
            for x in range(TILE_WIDTH):
                cel[y * TILE_WIDTH + x] = pixels[y * stride + tile * TILE_WIDTH + x]
        cels.append(cel)
    cels.append(draw_box(TILE_WIDTH, TILE_HEIGHT, CURSOR_INDEX))
    cels.append(draw_box(TILE_WIDTH, TILE_HEIGHT, PICKED_INDEX))

    transparent = [TILE_TRANSPARENT] * TILE_COUNT + [BOX_TRANSPARENT] * 2

    assert len(cels) == len(transparent)

    output = bytearray()
    output += MAGIC
    output += struct.pack("<HHH", TILE_WIDTH, TILE_HEIGHT, len(cels))
    output += bytes(palette)
    output += bytes(transparent)
    for cel in cels:
        output += bytes(cel)

    with open(output_path, "wb") as file:
        file.write(output)

    print(
        "png_to_spr: %d cels of %dx%d, %d bytes -> %s"
        % (len(cels), TILE_WIDTH, TILE_HEIGHT, len(output), output_path)
    )


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: png_to_spr.py <sheet.png> <output.spr>")
    main(sys.argv[1], sys.argv[2])
