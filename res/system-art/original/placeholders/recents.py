#!/usr/bin/env python3
"""RECENTS, the home's Recents shelf: a placeholder drawn by this script
(1.0.6), a clock with ZyperOS's lime rim on a 32-cell grid, each cell 8
pixels of the 256 one. Two poses, 400 ms each, as Favorites' heart beats:
the minute hand at twelve, then at one. Original work: shapes from code, no
source image. A real design comes later.

    recents.py OUT_DIR    writes RECENTS.png (the 512 x 256 atlas, two
                          poses) and RECENTS-still.png (the first, 256)
"""
import sys
from pathlib import Path

from PIL import Image

N, CELL = 32, 8
CX = CY = 15.5
INK = (28, 26, 36, 255)          # the outline and the hands
RIM = (190, 242, 60, 255)        # ZyperOS's lime
RIM_DARK = (104, 150, 22, 255)
RIM_LIGHT = (232, 255, 168, 255)
FACE = (246, 240, 224, 255)
FACE_SHADE = (221, 211, 189, 255)
SHINE = (255, 255, 255, 255)


def dist(x, y):
    return ((x + 0.5 - CX) ** 2 + (y + 0.5 - CY) ** 2) ** 0.5


def pose(minute):
    g = [[None] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            d = dist(x, y)
            side = (x + 0.5 - CX) + (y + 0.5 - CY)      # toward the lower right
            if d <= 8.6:
                g[y][x] = FACE_SHADE if side > 7.5 else FACE
            elif d <= 11.0:
                g[y][x] = RIM_DARK if side > 6.0 else RIM_LIGHT if side < -9.0 else RIM
            elif d <= 12.1:
                g[y][x] = INK
    # The rim's inner line, so the face reads as set into it
    for y in range(N):
        for x in range(N):
            if 8.6 < dist(x, y) <= 9.5:
                g[y][x] = INK
    # A shine on the face, upper left
    for x, y in ((10, 10), (11, 9), (9, 11)):
        g[y][x] = SHINE
    # The four marks: 12, 3, 6, 9
    for x, y in ((15, 8), (22, 15), (15, 22), (8, 15)):
        g[y][x] = INK
    # The hour hand, toward ten
    for x, y in ((14, 15), (13, 14), (12, 14)):
        g[y][x] = INK
    # The minute hand: twelve, or one
    hand = [(15, 14), (15, 13), (15, 12), (15, 11), (15, 10)] if minute == 0 else \
        [(16, 14), (16, 13), (17, 12), (17, 11), (18, 10)]
    for x, y in hand:
        g[y][x] = INK
    # The pin
    g[15][15] = RIM_DARK
    img = Image.new('RGBA', (N * CELL, N * CELL), (0, 0, 0, 0))
    px = img.load()
    for y in range(N):
        for x in range(N):
            if g[y][x]:
                for j in range(CELL):
                    for i in range(CELL):
                        px[x * CELL + i, y * CELL + j] = g[y][x]
    return img


def main():
    out = Path(sys.argv[1])
    a, b = pose(0), pose(1)
    atlas = Image.new('RGBA', (512, 256), (0, 0, 0, 0))
    atlas.paste(a, (0, 0))
    atlas.paste(b, (256, 0))
    atlas.save(out / 'RECENTS.png', optimize=True)
    a.save(out / 'RECENTS-still.png', optimize=True)


if __name__ == '__main__':
    main()
