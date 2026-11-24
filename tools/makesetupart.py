"""Generate the side picture of the installer's welcome and finish pages.

    python tools/makesetupart.py        # writes assets/setup-side.bmp

164x314, the size NSIS's Modern UI lays out. It is the app's own look: the
title-bar mark from makeicon.py on the app background, over a strip of the
arena -- the core's cells, a few of them taken by two warriors' colours.
Colours are the theme's defaults (Col:: in src/main.cpp).
"""
import os
import random
import struct
import sys

sys.dont_write_bytecode = True     # no __pycache__ in tools/ from the import below
from makeicon import render

W, H = 164, 314

BG          = (0x0B, 0x0D, 0x12)   # Col::Bg0
ARENA_CORE  = (0x17, 0x1B, 0x24)   # Col::ArenaCore
ACCENT      = (0x5B, 0x8C, 0xFF)   # Col::Accent
ACCENT2     = (0x36, 0xD3, 0xC0)   # Col::Accent2

ICON   = 64
ICON_Y = 44
CELL, GAP = 6, 2                   # arena cells and the lines between them
FADE_TOP, FADE_BOT = 120, 250      # the arena fades in over these rows


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t + 0.5) for i in range(3))


def main():
    px = [[BG] * W for _ in range(H)]

    # the arena: a grid of cells, fading in from the top
    rng = random.Random(8086)
    pitch = CELL + GAP
    cols = (W - GAP) // pitch
    x_off = (W - (cols * pitch - GAP)) // 2
    owner = {}
    # two warriors, each a few runs of written cells somewhere in the core
    for colour, runs in ((ACCENT, 8), (ACCENT2, 7)):
        for _ in range(runs):
            r = rng.randrange(FADE_TOP // pitch + 3, H // pitch)
            c = rng.randrange(0, cols)
            for k in range(rng.randrange(2, 7)):
                owner[(r, c + k)] = colour
    for r in range(H // pitch + 1):
        y0 = r * pitch
        for c in range(cols):
            x0 = x_off + c * pitch
            cell = owner.get((r, c), ARENA_CORE)
            for y in range(y0, min(y0 + CELL, H)):
                t = (y - FADE_TOP) / float(FADE_BOT - FADE_TOP)
                t = max(0.0, min(1.0, t))
                t = t * t * (3 - 2 * t)                    # smoothstep
                if cell is not ARENA_CORE:
                    t *= 0.85
                for x in range(x0, x0 + CELL):
                    px[y][x] = mix(BG, cell, t)

    # the mark, alpha-blended on top
    rgba = render(ICON)
    ix = (W - ICON) // 2
    for y in range(ICON):
        for x in range(ICON):
            i = (y * ICON + x) * 4
            a = rgba[i + 3] / 255.0
            if a:
                under = px[ICON_Y + y][ix + x]
                px[ICON_Y + y][ix + x] = mix(under, (rgba[i], rgba[i + 1], rgba[i + 2]), a)

    # 24-bit BMP, bottom-up, rows padded to 4 bytes
    stride = (W * 3 + 3) & ~3
    data = bytearray()
    for y in range(H - 1, -1, -1):
        row = bytearray()
        for r, g, b in px[y]:
            row += bytes((b, g, r))
        data += row + b'\x00' * (stride - len(row))
    info = struct.pack('<IiiHHIIiiII', 40, W, H, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    head = struct.pack('<2sIHHI', b'BM', 14 + len(info) + len(data), 0, 0, 14 + len(info))

    here = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(os.path.dirname(here), 'assets', 'setup-side.bmp')
    with open(out_path, 'wb') as f:
        f.write(head + info + data)
    print('wrote %s  (%dx%d)' % (out_path, W, H))


if __name__ == '__main__':
    main()
