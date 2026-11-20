#!/usr/bin/env python3
"""SCREENSHOTS, the home's Screenshots group: a placeholder drawn by this
script (1.1.0), a camera on a 32-cell grid, each cell 8 pixels of the 256
one, its lens ringed in ZyperOS's lime. Two poses, 400 ms each, as
Favorites' heart beats: the camera at rest, then its flash firing. Original
work: shapes from code, no source image. A real design comes later.

    screenshots-card.py OUT_DIR    writes SCREENSHOTS.png (the 512 x 256 atlas,
                                   two poses) and SCREENSHOTS-still.png (the
                                   first, 256)
"""
import math
import sys
from pathlib import Path

from PIL import Image

N, CELL = 32, 8
INK = (28, 26, 36, 255)          # the outline
SHELL = (132, 136, 146, 255)
SHELL_LIGHT = (176, 180, 190, 255)
SHELL_DARK = (94, 98, 108, 255)
LIME = (190, 242, 60, 255)       # ZyperOS's, the lens's rim
GLASS = (40, 52, 79, 255)
GLINT = (120, 150, 200, 255)
PAPER = (246, 240, 224, 255)     # the flash's window, unlit
WHITE = (255, 255, 255, 255)
RAY = (255, 242, 168, 255)       # the flash's light
# The flash's light leaving the camera: left, up, and between them
RAYS = [(1, 15), (2, 15), (6, 9), (7, 9), (6, 7), (7, 7), (2, 10), (1, 9)]


def pose(lit):
    g = [[None] * N for _ in range(N)]
    # The body, its corners rounded, lit at the top and shaded at the foot
    for y in range(11, 27):
        for x in range(3, 29):
            edge_x, edge_y = x in (3, 28), y in (11, 26)
            if edge_x and edge_y:
                continue
            g[y][x] = INK if edge_x or edge_y else SHELL_LIGHT if y == 12 else SHELL_DARK if y == 25 else SHELL
    # The viewfinder's hump on top
    for y in range(8, 12):
        for x in range(10, 19):
            edge_x, edge_y = x in (10, 18), y == 8
            if edge_x and edge_y:
                continue
            g[y][x] = INK if edge_x or edge_y else SHELL_LIGHT if y == 9 else SHELL
    # The shutter button
    for x in range(22, 26):
        g[9][x] = INK
        g[10][x] = INK if x in (22, 25) else SHELL_DARK
    # The lens: dark glass, the lime rim, an outline
    for y in range(11, 27):
        for x in range(8, 25):
            d = math.hypot(x + .5 - 16.5, y + .5 - 19)
            if d <= 3.7:
                g[y][x] = GLASS
            elif d <= 5.5:
                g[y][x] = LIME
            elif d <= 6.5:
                g[y][x] = INK
    g[17][14] = WHITE if lit else GLINT
    g[17][15] = GLINT
    g[18][14] = GLINT
    # The flash's window, and its light when it fires
    for y in range(14, 17):
        for x in range(5, 9):
            g[y][x] = INK if x in (5, 8) or y in (14, 16) else WHITE if lit else PAPER
    if lit:
        for x, y in RAYS:
            g[y][x] = RAY
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
    a, b = pose(False), pose(True)
    atlas = Image.new('RGBA', (512, 256), (0, 0, 0, 0))
    atlas.paste(a, (0, 0))
    atlas.paste(b, (256, 0))
    atlas.save(out / 'SCREENSHOTS.png', optimize=True)
    a.save(out / 'SCREENSHOTS-still.png', optimize=True)


if __name__ == '__main__':
    main()
