"""Generate app.ico from the mark the title bar already draws.

The logo is a rounded accent square with a small dark square punched out of the
middle. Rendering it here rather than checking in a binary means the icon can be
re-tinted by editing two constants.

    python tools/makeicon.py            # writes assets/app.ico

Sizes 16/32/48/64 are stored as 32-bit BMPs (what every Windows shell reads) and
128/256 as PNG, which is how large icons are meant to be stored.
"""
import os
import struct
import zlib

ACCENT_TOP = (0x5C, 0x99, 0xFF)
ACCENT_BOT = (0x3E, 0x7C, 0xEE)
INNER      = (0x0E, 0x10, 0x14)

SIZES  = [16, 32, 48, 64, 128, 256]
AS_PNG = {128, 256}
SS     = 4          # supersampling factor


def inside_round_rect(px, py, x0, y0, x1, y1, r):
    cx = min(max(px, x0 + r), x1 - r)
    cy = min(max(py, y0 + r), y1 - r)
    dx, dy = px - cx, py - cy
    return dx * dx + dy * dy <= r * r


def render(n):
    """RGBA bytes, top-down, n x n."""
    hi = n * SS
    pad = hi * 0.055                       # a little air around the mark
    x0, y0, x1, y1 = pad, pad, hi - pad, hi - pad
    outer_r = (x1 - x0) * 0.235

    side = (x1 - x0) * 0.30                # the punched-out centre
    ix0 = (hi - side) * 0.5
    iy0 = (hi - side) * 0.5
    ix1, iy1 = ix0 + side, iy0 + side
    inner_r = side * 0.22

    out = bytearray(n * n * 4)
    for y in range(n):
        for x in range(n):
            ar = ag = ab = acc = 0.0
            for sy in range(SS):
                py = y * SS + sy + 0.5
                for sx in range(SS):
                    px = x * SS + sx + 0.5
                    if not inside_round_rect(px, py, x0, y0, x1, y1, outer_r):
                        continue
                    if inside_round_rect(px, py, ix0, iy0, ix1, iy1, inner_r):
                        r, g, b = INNER
                    else:
                        t = (py - y0) / max(1.0, y1 - y0)
                        r = ACCENT_TOP[0] + (ACCENT_BOT[0] - ACCENT_TOP[0]) * t
                        g = ACCENT_TOP[1] + (ACCENT_BOT[1] - ACCENT_TOP[1]) * t
                        b = ACCENT_TOP[2] + (ACCENT_BOT[2] - ACCENT_TOP[2]) * t
                    ar += r; ag += g; ab += b; acc += 1.0

            i = (y * n + x) * 4
            total = float(SS * SS)
            if acc == 0.0:
                continue                    # transparent
            a = acc / total
            out[i + 0] = int(ar / acc + 0.5)
            out[i + 1] = int(ag / acc + 0.5)
            out[i + 2] = int(ab / acc + 0.5)
            out[i + 3] = int(a * 255.0 + 0.5)
    return bytes(out)


def to_png(n, rgba):
    raw = b''.join(b'\x00' + rgba[y * n * 4:(y + 1) * n * 4] for y in range(n))

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data +
                struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))

    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', n, n, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9))
            + chunk(b'IEND', b''))


def to_bmp(n, rgba):
    # BITMAPINFOHEADER with a doubled height: the colour rows, then the AND mask
    hdr = struct.pack('<IiiHHIIiiII', 40, n, n * 2, 1, 32, 0, n * n * 4, 0, 0, 0, 0)
    rows = []
    for y in range(n - 1, -1, -1):          # bottom-up
        row = bytearray()
        for x in range(n):
            i = (y * n + x) * 4
            row += bytes((rgba[i + 2], rgba[i + 1], rgba[i + 0], rgba[i + 3]))
        rows.append(bytes(row))
    mask_stride = ((n + 31) // 32) * 4      # 1bpp, rows padded to 4 bytes
    return hdr + b''.join(rows) + b'\x00' * (mask_stride * n)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(os.path.dirname(here), 'assets', 'app.ico')

    images = []
    for n in SIZES:
        rgba = render(n)
        images.append((n, to_png(n, rgba) if n in AS_PNG else to_bmp(n, rgba)))

    header = struct.pack('<HHH', 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries, blobs = b'', b''
    for n, data in images:
        entries += struct.pack('<BBBBHHII', n & 0xFF, n & 0xFF, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        blobs += data

    with open(out_path, 'wb') as f:
        f.write(header + entries + blobs)
    print('wrote %s  (%d sizes, %d bytes)' % (out_path, len(images), len(header + entries + blobs)))


if __name__ == '__main__':
    main()
