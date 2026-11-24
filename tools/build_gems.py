#!/usr/bin/env python3
"""Build the gem sprite sheets from the source renders.

Each source sheet contains a 16x15 grid of 256x256 renders. Every second
frame is scaled to 24x24. Only pixels inside the gem animate. The mount
comes from a single unlit frame for each colour.

Each colour retains 120 cels, with 60 reserved for glimmer and 60 for idle.
The six gem sheets and the two-cel UI sheet share one VGA palette.

Usage: build_gems.py <source-dir> <output-dir>
"""

import struct
import sys

from PIL import Image, ImageDraw, ImageFilter

CELL = 256
COLUMNS = 16
ROWS = 15
CELS_PER_RUN = 60
OUT = 24

COLORS = ["blue", "cyan", "green", "purple", "red", "yellow"]

# Zero-based, row-major source cels selected from the quiet part of each
# idle cycle. Keeping the frame choices explicit makes the base art stable.
BASE_CELS = {
    "blue": 142,
    "cyan": 128,
    "green": 130,
    "purple": 182,
    "red": 190,
    "yellow": 128,
}

# Hand-clean the stray reflections just outside the cuts. Coordinates are
# in the final 24x24 cel, paired with a nearby clean mount pixel.
MOUNT_TOUCHUPS = {
    "cyan": (((18, 10), (19, 10)), ((18, 13), (19, 13))),
    "purple": (((18, 14), (19, 15)), ((19, 14), (20, 14))),
    "yellow": (((18, 13), (19, 13)),),
}

# Minimum channel separation for a pixel to read as part of the jewel.
# The red mount already has warm shadows, so red needs a stronger key.
JEWEL_CHROMA = {
    "blue": 20,
    "cyan": 15,
    "green": 18,
    "purple": 19,
    "red": 42,
    "yellow": 24,
}

# Palette indices reserved for the game rather than the art.
COLOR_COUNT = 253
BACKGROUND_INDEX = 253
CURSOR_INDEX = 254
PICKED_INDEX = 255

# A hard silhouette removes the faint resampling fringe. Empty pixels stay
# opaque in the renderer, where the board's matte is cheaper than masking.
GEM_TRANSPARENT = 254  # never present in gem cels
BOX_TRANSPARENT = 0
ALPHA_CUTOFF = 96

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


def load_frames(path, colour):
    """Composite the original animation onto the mount from a quiet frame."""
    image = Image.open(path).convert("RGBA")
    if image.width != CELL * COLUMNS or image.height != CELL * ROWS:
        raise SystemExit(
            "%s is %dx%d, expected %dx%d"
            % (path, image.width, image.height, CELL * COLUMNS, CELL * ROWS)
        )

    def read_tile(index):
        row, column = divmod(index, COLUMNS)
        box = (column * CELL, row * CELL, (column + 1) * CELL, (row + 1) * CELL)
        return image.crop(box).resize((OUT, OUT), Image.Resampling.LANCZOS)

    base = read_tile(BASE_CELS[colour])
    mask = animation_mask(base, colour)
    for pixel, clean_mount_pixel in MOUNT_TOUCHUPS.get(colour, ()):
        mask.putpixel(pixel, 0)
        base.putpixel(pixel, base.getpixel(clean_mount_pixel))
    mount = paint_tile(base, mask)
    # Inset the animation where the render's reflections reach the cut edge.
    glow_mask = mask
    if colour in ("blue", "cyan", "purple", "yellow"):
        glow_mask = mask.filter(ImageFilter.MinFilter(3))
    source_cels = COLUMNS * ROWS
    step = source_cels // (2 * CELS_PER_RUN)
    return [
        Image.composite(paint_tile(read_tile(index), mask), mount, glow_mask)
        for index in range(0, source_cels, step)
    ]


def jewel_chroma(colour, red, green, blue):
    if colour == "blue":
        return blue - max(red, green)
    if colour == "cyan":
        return min(green, blue) - red
    if colour == "green":
        return green - max(red, blue)
    if colour == "purple":
        return min(red, blue) - green
    if colour == "red":
        return red - max(green, blue)
    return min(red, green) - blue


def jewel_mask(pixels, colour):
    """Find the rendered jewel without mistaking coloured mount details for it."""
    centre = (OUT - 1) / 2
    cutoff = JEWEL_CHROMA[colour]
    mask = Image.new("L", (OUT, OUT))
    mask.putdata([
        255 if (i % OUT - centre) ** 2 + (i // OUT - centre) ** 2 <= 8.5 ** 2
        and alpha >= ALPHA_CUTOFF and jewel_chroma(colour, red, green, blue) >= cutoff
        else 0
        for i, (red, green, blue, alpha) in enumerate(pixels)
    ])
    # Close one-pixel gaps without flattening the dark centres of the cuts.
    return mask.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MinFilter(3))


def animation_mask(base, colour):
    rgba = iter(base.tobytes())
    mask = jewel_mask(zip(rgba, rgba, rgba, rgba), colour)
    # Flood from the corner to distinguish the mount from dark pixels
    # enclosed by the jewel's outline.
    ImageDraw.floodfill(mask, (0, 0), 128)
    return mask.point(lambda value: 0 if value == 128 else 255)


def paint_tile(tile, mask):
    """Retain the rendered facets and give the mount and jewel distinct values."""
    rgba = iter(tile.tobytes())
    pixels = list(zip(rgba, rgba, rgba, rgba))
    edge = mask.filter(ImageFilter.MaxFilter(3))
    lightness = tile.convert("L")
    softened = lightness.filter(ImageFilter.GaussianBlur(0.8))
    output = []
    for (red, green, blue, alpha), jewel, rim, light, soft in zip(
        pixels, mask.tobytes(), edge.tobytes(), lightness.tobytes(), softened.tobytes()
    ):
        if alpha < ALPHA_CUTOFF:
            output.append((0, 0, 0, 0))
            continue
        if jewel:
            # Sharpen the existing reflections and group nearby shades into
            # discrete values to retain the shape of the original facets.
            peak = max(red, green, blue)
            sharpened = peak * 1.26 + (light - soft) * 0.85
            value = min(255, max(0, round(sharpened / 16) * 16))
            rgb = tuple(max(0, value - round((peak - channel) * 1.15))
                        for channel in (red, green, blue))
        else:
            # Keep the source hue and highlights on the warm mount, with an
            # extra shadow immediately outside the gem to suggest a setting.
            scale = 0.53 if rim else 0.83
            rgb = tuple(round((channel * 0.8 + light * 0.2) * scale / 8) * 8
                        for channel in (red, green, blue))
        output.append((*rgb, 255))
    painted = Image.new("RGBA", tile.size)
    painted.putdata(output)
    return painted


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
    # Fill unused quantizer entries with a duplicate art colour. The actual
    # UI colours belong only in the palette sent to VGA.
    holder.putpalette(palette[:COLOR_COUNT * 3] + palette[:3] * 3)
    return palette, holder


def index_tile(tile, palette_image):
    """Index one painted tile and assign its empty pixels to the board matte."""
    indexed = on_black(tile).quantize(
        palette=palette_image, dither=Image.Dither.NONE
    )
    cel = bytearray(indexed.tobytes())
    for i, alpha in enumerate(tile.getchannel("A").tobytes()):
        if not alpha:
            cel[i] = BACKGROUND_INDEX
        elif cel[i] >= COLOR_COUNT:
            cel[i] = 0  # the three duplicate entries in the quantizer
    return cel


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
    frames = {
        colour: load_frames("%s/%s.png" % (source_dir, colour), colour)
        for colour in COLORS
    }
    palette, palette_image = build_palette(
        [tile for sequence in frames.values() for tile in sequence]
    )

    for colour, sequence in frames.items():
        cels = [index_tile(tile, palette_image) for tile in sequence]
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
