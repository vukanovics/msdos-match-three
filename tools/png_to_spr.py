#!/usr/bin/env python3
"""Convert match-three art into the engine's .spr format.

The DJGPP target has no PNG library, so art is converted on the host into a
small binary the engine reads at load time, rather than being compiled into the
executable.

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

A sheet is a selection of cels from one source image, so several sheets cut from
the same art share one palette, which is what the engine requires (Mode X shows
one palette at a time).

Usage: png_to_spr.py <sheet.png> <output.spr> [cel...]

    cel is "a:b" for tiles a up to b, or "boxes" for the two UI rectangles.
    With no cel arguments, all tiles and then the boxes are written.

    png_to_spr.py art.png TILES.SPR 0:6
    png_to_spr.py art.png UI.SPR boxes
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
# The UI boxes are hollow rectangles, so their interior is the key.
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


def pick_cels(pixels, stride, specs):
    """Turns cel specs into (cel data, transparent index) pairs."""
    cels = []
    transparent = []

    for spec in specs:
        if spec == "boxes":
            cels.append(draw_box(TILE_WIDTH, TILE_HEIGHT, CURSOR_INDEX))
            transparent.append(BOX_TRANSPARENT)
            cels.append(draw_box(TILE_WIDTH, TILE_HEIGHT, PICKED_INDEX))
            transparent.append(BOX_TRANSPARENT)
            continue

        first, _, last = spec.partition(":")
        try:
            start = int(first)
            end = int(last) if last else TILE_COUNT
        except ValueError:
            raise SystemExit("a cel is 'a:b' or 'boxes', not '%s'" % spec)
        if start < 0 or end > TILE_COUNT or start >= end:
            raise SystemExit(
                "cel range '%s' is outside 0:%d" % (spec, TILE_COUNT)
            )

        for tile in range(start, end):
            cel = bytearray(TILE_WIDTH * TILE_HEIGHT)
            for y in range(TILE_HEIGHT):
                for x in range(TILE_WIDTH):
                    cel[y * TILE_WIDTH + x] = pixels[
                        y * stride + tile * TILE_WIDTH + x
                    ]
            cels.append(cel)
            transparent.append(TILE_TRANSPARENT)

    return cels, transparent


def main(sprite_path, output_path, specs):
    sheet = Image.open(sprite_path).convert("RGBA")
    if sheet.height != TILE_HEIGHT or sheet.width != TILE_WIDTH * TILE_COUNT:
        raise SystemExit(
            "expected a %dx%d sheet, got %dx%d"
            % (TILE_WIDTH * TILE_COUNT, TILE_HEIGHT, sheet.width, sheet.height)
        )

    # Composite the transparency onto black, then reduce to a shared palette.
    background = Image.new("RGBA", sheet.size, (0, 0, 0, 255))
    sheet = Image.alpha_composite(background, sheet).convert("RGB")
    sheet = sheet.quantize(colors=COLOR_COUNT, method=Image.Quantize.MEDIANCUT)

    palette = sheet.getpalette()[: COLOR_COUNT * 3]
    palette += [0] * (PALETTE_SIZE - len(palette))
    palette[BACKGROUND_INDEX * 3:BACKGROUND_INDEX * 3 + 3] = [0, 0, 0]
    palette[CURSOR_INDEX * 3:CURSOR_INDEX * 3 + 3] = [255, 255, 255]
    palette[PICKED_INDEX * 3:PICKED_INDEX * 3 + 3] = [255, 0, 0]

    cels, transparent = pick_cels(sheet.tobytes(), sheet.width, specs)
    if not cels:
        raise SystemExit("no cels selected for %s" % output_path)
    if len(cels) != len(transparent):
        raise SystemExit("internal: cel and transparency counts differ")

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
    if len(sys.argv) < 3:
        raise SystemExit("usage: png_to_spr.py <sheet.png> <output.spr> [cel...]")
    arguments = sys.argv[3:] or ["0:%d" % TILE_COUNT, "boxes"]
    main(sys.argv[1], sys.argv[2], arguments)
