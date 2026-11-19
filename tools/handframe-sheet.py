#!/usr/bin/env python3
"""The handheld frames' check sheet (src/handframe.h, DESIGN.md 11.55): every
frame at every placement on the Pixel 2's 640x480 and the Brick's 1024x768,
a stand-in game in its window, and over it the model's own outlines one
pixel wide (tools/handframe-sheet.c --outline), so an edge off its line
shows; and the Game Boy Color's six colors. Writes OUT/handframe-sheet.png
and one PNG a frame beside it. Standard library only.

    python3 tools/handframe-sheet.py OUT
"""
import os, struct, subprocess, sys, tempfile, zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def ppm(path):
    with open(path, "rb") as f:
        data = f.read()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]


def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")
    os.makedirs(out, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="handframe-")
    tool = os.path.join(tmp, "sheet")
    subprocess.check_call(["cc", "-std=gnu11", "-O2", "-o", tool, os.path.join(ROOT, "tools/handframe-sheet.c"),
                           os.path.join(ROOT, "src/handframe.c"), os.path.join(ROOT, "src/handframe_models.c"), "-lm"])
    subprocess.check_call([tool, tmp, "--outline"], stdout=subprocess.DEVNULL)
    frames = sorted(n for n in os.listdir(tmp) if n.endswith(".ppm"))
    # each frame alone, and a sheet of all at half size, four across
    cells = []
    for n in frames:
        w, h, rgb = ppm(os.path.join(tmp, n))
        png(os.path.join(out, n[:-4] + ".png"), w, h, rgb)
        # down to 512 wide, nearest, so lines stay one colour
        k = w // 512 if w >= 1024 else 1
        sw, sh = (w // k, h // k) if k > 1 else (w * 512 // w, h * 512 // w)
        small = bytearray(sw * sh * 3)
        for y in range(sh):
            sy = y * h // sh
            for x in range(sw):
                sx = x * w // sw
                small[(y * sw + x) * 3:(y * sw + x) * 3 + 3] = rgb[(sy * w + sx) * 3:(sy * w + sx) * 3 + 3]
        cells.append((sw, sh, bytes(small)))
    cw, ch, cols = 512, 384, 4
    rows = (len(cells) + cols - 1) // cols
    W, H = cols * (cw + 8) + 8, rows * (ch + 8) + 8
    sheet = bytearray(b"\x33" * (W * H * 3))
    for i, (sw, sh, rgb) in enumerate(cells):
        ox, oy = 8 + (i % cols) * (cw + 8), 8 + (i // cols) * (ch + 8)
        for y in range(min(sh, ch)):
            sheet[((oy + y) * W + ox) * 3:((oy + y) * W + ox + min(sw, cw)) * 3] = rgb[y * sw * 3:(y * sw + min(sw, cw)) * 3]
    png(os.path.join(out, "handframe-sheet.png"), W, H, bytes(sheet))
    print("handframe-sheet: %d frames, %s" % (len(cells), os.path.join(out, "handframe-sheet.png")))


if __name__ == "__main__":
    main()
