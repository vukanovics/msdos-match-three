#!/usr/bin/env python3
"""Build the gem sprite sheets from the source renders.

The source art is a 16x15 grid of 256x256 cells per colour, read left to right
and top to bottom, at 60 frames per second. The game runs at 60 ticks and needs
4 to 8 MB of RAM in total, so:

  - every second frame is kept, which is 30 animation frames per second
  - each frame is scaled to 24x24, the tile size the board uses
  - one palette is built from all six colours, because Mode X shows one palette
    at a time and the engine rejects a scene whose sheets disagree

Each colour becomes one .spr of 120 cels, plus a two cel UI sheet whose boxes
are drawn here rather than in the engine.

Usage: build_gems.py <source-dir> <output-dir>
"""

import struct
import sys

from PIL import Image

CELL = 256
COLUMNS = 16
ROWS = 15
KEEP_EVERY = 2
OUT = 24

COLORS = ["blue", "cyan", "green", "purple", "red", "yellow"]

# Palette indices reserved for the game rather than the art.
COLOR_COUNT = 253
BACKGROUND_INDEX = 253
CURSOR_INDEX = 254
PICKED_INDEX = 255

# The gems are drawn opaque, with the transparent corners composited onto the
# board's background colour. A rounded cel costs the renderer a per-row mask on
# every corner row, which measured at three milliseconds a frame - the difference
# between holding 60 Hz and not. The corners are the background colour, so on a
# board of that colour they are invisible; only a gem sliding over another shows
# them, for the fifth of a second a swap takes.
#
# The palette reserves 253 for the background, and the gems are quantised into
# 0..252, so 253 is exactly what a transparent pixel becomes.
GEM_OPAQUE_ON_BACKGROUND = True
GEM_TRANSPARENT = 254  # never present, which is what makes the cel opaque
BOX_TRANSPARENT = 0

ALPHA_CUTOFF = 128

BOX_THICKNESS = 2

MAGIC = b"SPR1"
PALETTE_SIZE = 256 * 3


def write_spr(path, width, height, palette, cels, transparent):
    output = bytearray()
    output += MAGIC
    output += struct.pack("<HHH", width, height, len(cels))
    output += bytes(palette)
    output += bytes(transparent)
    for cel in cels:
        output += bytes(cel)
    with open(path, "wb") as file:
        file.write(output)
    print(
        "  %-34s %3d cels %dx%d  %7d bytes"
        % (path, len(cels), width, height, len(output))
    )


def load_frames(path):
    """The selected cells of one source sheet, scaled to the tile size."""
    image = Image.open(path).convert("RGBA")
    if image.width != CELL * COLUMNS or image.height != CELL * ROWS:
        raise SystemExit(
            "%s is %dx%d, expected %dx%d"
            % (path, image.width, image.height, CELL * COLUMNS, CELL * ROWS)
        )

    frames = []
    for row in range(ROWS):
        for column in range(COLUMNS):
            box = (
                column * CELL,
                row * CELL,
                (column + 1) * CELL,
                (row + 1) * CELL,
            )
            frames.append(image.crop(box))
    return [
        frame.resize((OUT, OUT), Image.LANCZOS) for frame in frames[::KEEP_EVERY]
    ]


def on_black(frame):
    background = Image.new("RGBA", frame.size, (0, 0, 0, 255))
    return Image.alpha_composite(background, frame).convert("RGB")


def build_palette(frames):
    """One palette for every colour, from the art that is actually kept."""
    columns = 32
    rows = (len(frames) + columns - 1) // columns
    montage = Image.new("RGB", (columns * OUT, rows * OUT), (0, 0, 0))
    for index, frame in enumerate(frames):
        montage.paste(
            on_black(frame), ((index % columns) * OUT, (index // columns) * OUT)
        )

    reduced = montage.quantize(colors=COLOR_COUNT, method=Image.Quantize.MEDIANCUT)
    palette = reduced.getpalette()[: COLOR_COUNT * 3]
    palette += [0] * (PALETTE_SIZE - len(palette))
    palette[BACKGROUND_INDEX * 3:BACKGROUND_INDEX * 3 + 3] = [0, 0, 0]
    palette[CURSOR_INDEX * 3:CURSOR_INDEX * 3 + 3] = [255, 255, 255]
    palette[PICKED_INDEX * 3:PICKED_INDEX * 3 + 3] = [255, 0, 0]

    holder = Image.new("P", (1, 1))
    holder.putpalette(palette)
    return palette, holder


def index_frames(frames, palette_image):
    """Palette indices for each frame, with empty space made transparent."""
    cels = []
    for frame in frames:
        indexed = on_black(frame).quantize(
            palette=palette_image, dither=Image.Dither.NONE
        )
        cel = bytearray(indexed.tobytes())
        if not GEM_OPAQUE_ON_BACKGROUND:
            alpha = frame.getchannel("A").tobytes()
            for i, value in enumerate(alpha):
                if value < ALPHA_CUTOFF:
                    cel[i] = GEM_TRANSPARENT
        cels.append(cel)
    return cels


def draw_box(width, height, colour):
    cel = bytearray([BOX_TRANSPARENT]) * (width * height)
    for t in range(BOX_THICKNESS):
        for x in range(t, width - t):
            cel[t * width + x] = colour
            cel[(height - 1 - t) * width + x] = colour
        for y in range(t, height - t):
            cel[y * width + t] = colour
            cel[y * width + width - 1 - t] = colour
    return cel


def main(source_dir, output_dir):
    frames = {}
    every = []
    for colour in COLORS:
        frames[colour] = load_frames("%s/%s.png" % (source_dir, colour))
        every.extend(frames[colour])
    print(
        "loaded %d colours, %d frames each after halving, scaled to %dx%d"
        % (len(COLORS), len(frames[COLORS[0]]), OUT, OUT)
    )

    palette, palette_image = build_palette(every)

    for colour in COLORS:
        cels = index_frames(frames[colour], palette_image)
        write_spr(
            "%s/gem_%s.spr" % (output_dir, colour),
            OUT,
            OUT,
            palette,
            cels,
            [GEM_TRANSPARENT] * len(cels),
        )

    boxes = [draw_box(OUT, OUT, CURSOR_INDEX), draw_box(OUT, OUT, PICKED_INDEX)]
    write_spr(
        "%s/ui.spr" % output_dir,
        OUT,
        OUT,
        palette,
        boxes,
        [BOX_TRANSPARENT] * len(boxes),
    )


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: build_gems.py <source-dir> <output-dir>")
    main(sys.argv[1], sys.argv[2])
