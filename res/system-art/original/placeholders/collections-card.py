#!/usr/bin/env python3
"""COLLECTIONS, the home's Collections group: a placeholder drawn by this
script (1.0.7), a stack of three cartridges on a 32-cell grid, each cell 8
pixels of the 256 one, the top one's label in ZyperOS's lime. Two poses,
400 ms each, as Favorites' heart beats: the stack at rest, then its top
cartridge lifted off it, as a hand picks one. Original work: shapes from
code, no source image. A real design comes later.

    collections-card.py OUT_DIR    writes COLLECTIONS.png (the 512 x 256 atlas,
                                   two poses) and COLLECTIONS-still.png (the
                                   first, 256)
"""
import sys
from pathlib import Path

from PIL import Image

N, CELL = 32, 8
INK = (28, 26, 36, 255)          # the outline and the grip's ridges
SHELL = (132, 136, 146, 255)
SHELL_LIGHT = (176, 180, 190, 255)
SHELL_DARK = (94, 98, 108, 255)
PAPER = (246, 240, 224, 255)     # the label's lit edge
LIME = (190, 242, 60, 255)       # ZyperOS's, the top cartridge's label
BLUE = (91, 143, 214, 255)
PURPLE = (138, 111, 192, 255)
W, H = 20, 7                     # a cartridge, lying flat, seen from the front


def cartridge(g, x0, y0, label):
    for y in range(y0, y0 + H):
        for x in range(x0, x0 + W):
            edge_x = x in (x0, x0 + W - 1)
            edge_y = y in (y0, y0 + H - 1)
            if edge_x and edge_y:
                continue                              # its corners rounded
            if edge_x or edge_y:
                g[y][x] = INK
            elif y == y0 + 1:
                g[y][x] = SHELL_LIGHT
            elif y == y0 + H - 2:
                g[y][x] = SHELL_DARK
            else:
                g[y][x] = SHELL
    # The label, its top edge lit
    for y in range(y0 + 2, y0 + H - 2):
        for x in range(x0 + 3, x0 + W - 8):
            g[y][x] = PAPER if y == y0 + 2 else label
    # The grip: three ridges at its right
    for x in (x0 + W - 6, x0 + W - 4):
        for y in range(y0 + 2, y0 + H - 2):
            g[y][x] = INK


def pose(lifted):
    g = [[None] * N for _ in range(N)]
    cartridge(g, 6, 21, BLUE)
    cartridge(g, 5, 15, PURPLE)
    cartridge(g, 7, 7 if lifted else 9, LIME)
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
    atlas.save(out / 'COLLECTIONS.png', optimize=True)
    a.save(out / 'COLLECTIONS-still.png', optimize=True)


if __name__ == '__main__':
    main()
