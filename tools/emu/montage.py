#!/usr/bin/env python3
"""Tile emulator screenshots (PNGs written by cemu.bmp2png, any integer scale) at native 320x240
into one grid image, to review several results in one picture.  Usage: montage.py out.png cols in1.png in2.png ..."""
import struct, sys, zlib


def read_png(path):
    d = open(path, 'rb').read()
    w, h = struct.unpack('>II', d[16:24])
    pos, idat = 8, b''
    while pos < len(d):
        n, = struct.unpack('>I', d[pos:pos + 4]); t = d[pos + 4:pos + 8]
        if t == b'IDAT':
            idat += d[pos + 8:pos + 8 + n]
        pos += 12 + n
    raw = zlib.decompress(idat)
    stride = w * 3 + 1
    return w, h, [raw[y * stride + 1:(y + 1) * stride] for y in range(h)]   # filter 0 only (our writer)


def main(out, cols, files):
    tiles = []
    for f in files:
        w, h, rows = read_png(f)
        s = w // 320
        tiles.append([bytes(b for x in range(0, w, s) for b in r[x * 3:x * 3 + 3]) for r in rows[::s]])
    rows_n = (len(tiles) + cols - 1) // cols
    W, H, gap = cols * 320 + (cols - 1) * 4, rows_n * 240 + (rows_n - 1) * 4, b'\x40\x40\x40'
    blank = [b'\x40\x40\x40' * 320] * 240
    lines = []
    for r in range(rows_n):
        row_tiles = [tiles[r * cols + c] if r * cols + c < len(tiles) else blank for c in range(cols)]
        for y in range(240):
            lines.append(b'\x00' + (gap * 4).join(t[y] for t in row_tiles))
        if r < rows_n - 1:
            lines += [b'\x00' + gap * W] * 4

    def chunk(t, p):
        return struct.pack('>I', len(p)) + t + p + struct.pack('>I', zlib.crc32(t + p) & 0xffffffff)
    open(out, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0))
                          + chunk(b'IDAT', zlib.compress(b''.join(lines), 9)) + chunk(b'IEND', b''))


if __name__ == '__main__':
    main(sys.argv[1], int(sys.argv[2]), sys.argv[3:])
