#!/usr/bin/env python3
"""The art of Splore's entry on the PICO-8 shelf: its icon and its covers.

    python3 tools/genpico8-art.py     writes res/pico8/icon.png,
                                      res/pico8/cover-light.png and
                                      res/pico8/cover-dark.png

What it draws: a magnifying glass (a brass rim, shaded and outlined, pale
blue glass with a white highlight, a wooden handle going down to the right)
held over a row of five small game cartridges standing on a wooden shelf.
Inside the lens one cartridge shows bigger, the red one under it, with a
heart on its label; two small sparkles beside the glass. It is pixel art on
a 64 x 64 grid in the home's style, as PortMaster's crane is (DESIGN.md
11.33): a dark outline round every part, two or three shades a material.

It is ZyperOS's own drawing. It uses no part of Lexaloffle's logo or art
and no trademarked imagery: the cartridges are generic ones (a body, a label
window, two grip lines), and their colors are from PICO-8's public 16-color
palette.

icon.png is the grid at 4x, 256 x 256, on nothing. The covers follow
cover() in res/system-art/original/utility-export.js, which draws
PortMaster's: 512 x 656 clipped to a 22 px rounded rect, the slab (#edece4
light, #171b18 dark), a 6 px band of the shelf's color (PICO-8's, #29adff)
across the top, the sun in light and the moon and a few stars in dark, then
the drawing at 8x at (0, 48). Where PortMaster's cover has water, this one
has a wooden desk across the cover's foot, and the cartridges stand on it in
place of the icon's short shelf.

Only the standard library: the PNGs are RGBA, 8-bit, not interlaced, every
row filter 0 and zlib level 9, so a run writes the same bytes every time.
"""
import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "res", "pico8")

GRID = 64                        # the drawing is 64 x 64 art pixels
ICON_SCALE = 4                   # icon.png: 256 x 256
COVER_W, COVER_H = 512, 656      # a generated card's size
COVER_SCALE = 8                  # the drawing at 512 x 512 ...
COVER_TOP = 48                   # ... at (0, 48), as utility-export.js does
COVER_RADIUS = 22


def rgb(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), 255)


# The outline, PortMaster's art's dark navy ink.
INK = rgb("#1b2231")
WHITE = rgb("#ffffff")

# Brass: the lens's rim and the handle's collar.
BRASS = {"L": rgb("#ffe58a"), "B": rgb("#f7b928"), "D": rgb("#c47a12")}
# Glass: pale blue, a little deeper at its lower right, a white highlight.
GLASS, GLASS_DEEP = rgb("#c4eafc"), rgb("#97d1f0")
# Wood: the handle, the shelf and the cover's desk.
WOOD = {"L": rgb("#e9b57a"), "B": rgb("#c47f45"), "D": rgb("#8f552c"),
        "G": rgb("#a8693a")}
# The label on every cartridge: PICO-8's white, and its shade.
LABEL, LABEL_SHADE = rgb("#fff1e8"), rgb("#e2cdbd")

# The cartridges: a PICO-8 palette color each, with a light and a dark of it.
CARTS = {
    "red":    {"L": rgb("#ff6a90"), "B": rgb("#ff004d"), "D": rgb("#b0003c")},
    "orange": {"L": rgb("#ffc860"), "B": rgb("#ffa300"), "D": rgb("#c96a00")},
    "yellow": {"L": rgb("#fff7a8"), "B": rgb("#ffec27"), "D": rgb("#d6a80c")},
    "green":  {"L": rgb("#78f592"), "B": rgb("#00e436"), "D": rgb("#00953c")},
    "blue":   {"L": rgb("#95dbff"), "B": rgb("#29adff"), "D": rgb("#1b6fc9")},
    "pink":   {"L": rgb("#ffc0d6"), "B": rgb("#ff77a8"), "D": rgb("#cf4a85")},
}
# The row on the shelf, left to right, and the little picture on each label.
ROW = [("blue", "orange"), ("red", "red"), ("yellow", "green"),
       ("green", "pink"), ("pink", "blue")]
LENS_CART = 1                    # the red one, under the lens

# The covers' grounds: the slab, the band, the sun, the moon and stars, all
# as utility-export.js has them, and the desk (light, dark).
SLAB = {False: rgb("#edece4"), True: rgb("#171b18")}
BAND = rgb("#29adff")
SUN, SUN_CORE = rgb("#f6cf69"), rgb("#ffe399")
MOON, STAR = rgb("#d9e1dd"), rgb("#95aaa4")
DESK = {
    False: {"top": rgb("#dfa86c"), "lip": rgb("#f0c690"),
            "front": rgb("#b8763f"), "grain": rgb("#9c5f33"),
            "shadow": rgb("#c38c55")},
    True:  {"top": rgb("#8a5a35"), "lip": rgb("#a4703f"),
            "front": rgb("#5e3a22"), "grain": rgb("#4b2d1a"),
            "shadow": rgb("#734a2b")},
}

# --- the parts, as fills; each gets its outline when it is placed ----------

# A small cartridge, 7 x 11 (9 x 13 outlined): its top corners cut, a
# highlight down its left and along its top, a shade down its right and along
# its foot, the label window with a two-pixel picture, two grip lines.
SMALL_CART = [
    ".LLLLB.",
    "LWWWWWD",
    "LWPPWwD",
    "LWPPWwD",
    "LwwwwwD",
    "LBBBBBD",
    "LgggggD",
    "LBBBBBD",
    "LgggggD",
    "LBBBBBD",
    "DDDDDDD",
]

# The same cartridge magnified, 11 x 16: the lens shows its label, a heart.
BIG_CART = [
    ".LLLLLLLLB.",
    "LLLLLLLLLBD",
    "LWWWWWWWWwD",
    "LWWHHWHHWwD",
    "LWHhHHHHHwD",
    "LWHHHHHHHwD",
    "LWWHHHHHWwD",
    "LWWWHHHWWwD",
    "LWWWWHWWWwD",
    "LwwwwwwwwwD",
    "LBBBBBBBBBD",
    "LgggggggggD",
    "LBBBBBBBBBD",
    "LgggggggggD",
    "LBBBBBBBBBD",
    "DDDDDDDDDDD",
]

SPARKLE_BIG = [
    "..Y..",
    "..Y..",
    "YYWYY",
    "..Y..",
    "..Y..",
]
SPARKLE_SMALL = [
    ".Y.",
    "YWY",
    ".Y.",
]
SPARKLE = {"Y": rgb("#ffec27"), "W": WHITE}

# Where things go on the grid.
CART_X, CART_Y = 14, 38          # the first cartridge's fill, top left
CART_PITCH = 8                   # neighbours share an outline
SHELF_X0, SHELF_X1 = 10, 56      # the shelf's fill, inclusive
SHELF_TOP, SHELF_FRONT, SHELF_FOOT = 49, 52, 55
LENS_X, LENS_Y = 20, 19          # the lens's centre
R_GLASS, R_RIM = 9.5, 13.5       # glass, then 1 of ink, then 3 of brass
HANDLE_W = 3                     # the handle's half width, in diagonals
COLLAR = (19, 26)                # the brass collar, along the diagonal
HANDLE_END = 45                  # where the wood ends, along the diagonal
BIG_CART_TOP = -6                # the magnified cartridge's top, from LENS_Y
# The glass's highlight, from the lens's centre: an arc at its upper left.
HIGHLIGHT = [(-8, -2), (-8, -3), (-7, -4), (-7, -5), (-6, -6), (-5, -7)]
SPARKLES = [(SPARKLE_BIG, 41, 8), (SPARKLE_SMALL, 49, 17)]
# The cover's desk, rows of the grid: its back edge, the lit lip along its
# front edge; its front goes on below the lip to the cover's foot.
DESK_BACK, DESK_LIP = 46, 55


class Layer:
    """Pixels of one part: {(x, y): color}."""

    def __init__(self):
        self.px = {}

    def set(self, x, y, c):
        self.px[(x, y)] = c

    def sprite(self, x0, y0, rows, colors):
        for j, row in enumerate(rows):
            for i, ch in enumerate(row):
                if ch != ".":
                    self.set(x0 + i, y0 + j, colors[ch])

    def outlined(self, ink=INK):
        """The part with a one-pixel outline round it, four-connected, so
        corners come out rounded the way pixel art draws them."""
        out = Layer()
        for (x, y) in self.px:
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (x + dx, y + dy) not in self.px:
                    out.set(x + dx, y + dy, ink)
        out.px.update(self.px)
        return out


class Grid:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.px = [[None] * w for _ in range(h)]

    def paste(self, layer):
        for (x, y), c in layer.px.items():
            if 0 <= x < self.w and 0 <= y < self.h:
                self.px[y][x] = c


def cart_colors(name, picture):
    c = dict(CARTS[name])
    c.update({"W": LABEL, "w": LABEL_SHADE, "g": c["D"],
              "P": CARTS[picture]["B"]})
    return c


def shelf():
    """The icon's short shelf: a top face the cartridges stand on, narrower
    at the back as seen from above, a front face with grain, its foot in
    shade."""
    lay = Layer()
    for y in range(SHELF_TOP, SHELF_FOOT + 1):
        inset = max(0, (SHELF_FRONT - y) // 2)
        for x in range(SHELF_X0 + inset, SHELF_X1 + 1 - inset):
            if y < SHELF_FRONT:
                c = WOOD["L"]
            elif y == SHELF_FOOT:
                c = WOOD["D"]
            else:
                c = WOOD["B"]
            lay.set(x, y, c)
    # The front face's grain: two dashes.
    for x0, x1 in ((12, 20), (36, 47)):
        for x in range(x0, x1):
            lay.set(x, SHELF_FRONT + 1, WOOD["G"])
    for x in range(26, 32):
        lay.set(x, SHELF_FRONT + 2, WOOD["G"])
    return lay.outlined()


def small_cart(i):
    name, picture = ROW[i]
    x0 = CART_X + CART_PITCH * i
    lay = Layer()
    lay.sprite(x0, CART_Y, SMALL_CART, cart_colors(name, picture))
    lay = lay.outlined()
    # Square feet: it stands flat on the shelf, and the row's foot is one
    # line.
    foot = CART_Y + len(SMALL_CART)
    lay.set(x0 - 1, foot, INK)
    lay.set(x0 + len(SMALL_CART[0]), foot, INK)
    return lay


def handle():
    """The handle on the lens's diagonal, down to the right: k across it
    (its upper side catches the light), s along it."""
    lay = Layer()
    for y in range(GRID):
        for x in range(GRID):
            dx, dy = x - LENS_X, y - LENS_Y
            k, s = dx - dy, dx + dy
            if s < COLLAR[0] or s > HANDLE_END:
                continue
            collar = s <= COLLAR[1]
            w = HANDLE_W + (1 if collar else 0)
            if abs(k) > w:
                continue
            # Round the end off.
            if s >= HANDLE_END - 1 and abs(k) == w:
                continue
            pal = BRASS if collar else WOOD
            if k == w or (k == w - 1 and not collar):
                c = pal["L"]
            elif k <= -w + (0 if collar else 1):
                c = pal["D"]
            else:
                c = pal["B"]
            if collar and s == COLLAR[1] - 1:
                c = pal["D"] if k < w else pal["B"]
            lay.set(x, y, c)
    return lay.outlined()


def lens():
    """The rim, the glass, the magnified cartridge and the highlight."""
    lay = Layer()
    big = Layer()
    bx = LENS_X - len(BIG_CART[0]) // 2
    name, picture = ROW[LENS_CART]
    colors = cart_colors(name, picture)
    colors.update({"H": CARTS[picture]["B"], "h": WHITE})
    big.sprite(bx, LENS_Y + BIG_CART_TOP, BIG_CART, colors)
    big = big.outlined()
    span = int(R_RIM) + 1
    for y in range(LENS_Y - span, LENS_Y + span + 1):
        for x in range(LENS_X - span, LENS_X + span + 1):
            dx, dy = x - LENS_X, y - LENS_Y
            d = math.hypot(dx, dy)
            if d > R_RIM:
                continue
            if d > R_GLASS + 1:
                # The brass, a ring lit from the upper left: its outer two
                # rows light there and dark at the lower right, its inner
                # row the other way round, as a rounded rim's bevel is.
                light = -(dx + dy) / (d * math.sqrt(2))
                outer = d > R_GLASS + 2
                if light > 0.4:
                    c = BRASS["L"] if outer else BRASS["B"]
                elif light < -0.4:
                    c = BRASS["D"] if outer else BRASS["B"]
                else:
                    c = BRASS["B"]
            elif d > R_GLASS:
                c = INK
            else:
                c = big.px.get((x, y))
                if c is None:
                    deep = (dx + dy) / max(d, 1) > 0.5 and d > R_GLASS - 2
                    c = GLASS_DEEP if deep else GLASS
            lay.set(x, y, c)
    for dx, dy in HIGHLIGHT:
        lay.set(LENS_X + dx, LENS_Y + dy, WHITE)
    return lay.outlined()


def sparkle(rows, x, y):
    lay = Layer()
    lay.sprite(x, y, rows, SPARKLE)
    return lay.outlined()


def drawing(with_shelf=True):
    g = Grid(GRID, GRID)
    if with_shelf:
        g.paste(shelf())
    for i in range(len(ROW)):
        g.paste(small_cart(i))
    g.paste(handle())
    g.paste(lens())
    for rows, x, y in SPARKLES:
        g.paste(sparkle(rows, x, y))
    return g


# --- the files -------------------------------------------------------------

def png(path, w, h, rows):
    """rows: h bytes objects of w RGBA pixels each."""
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    data = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) +
            chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


def scaled_rows(grid, scale):
    clear = bytes(4)
    rows = []
    for line in grid.px:
        row = b"".join((bytes(c) if c else clear) * scale for c in line)
        rows.extend([row] * scale)
    return rows


def icon():
    g = drawing()
    return scaled_rows(g, ICON_SCALE)


def corner_alpha(x, y):
    """Coverage of pixel (x, y) by the cover's rounded rect, 4 x 4 samples."""
    r = COVER_RADIUS
    cx = r if x < r else (COVER_W - r if x >= COVER_W - r else None)
    cy = r if y < r else (COVER_H - r if y >= COVER_H - r else None)
    if cx is None or cy is None:
        return 255
    n = 0
    for j in range(4):
        for i in range(4):
            px, py = x + (i + 0.5) / 4, y + (j + 0.5) / 4
            if (px - cx) ** 2 + (py - cy) ** 2 <= r * r:
                n += 1
    return round(255 * n / 16)


def cover(dark):
    W, H = COVER_W, COVER_H
    img = [[SLAB[dark]] * W for _ in range(H)]

    def fill(x, y, w, h, c):
        for yy in range(max(y, 0), min(y + h, H)):
            row = img[yy]
            for xx in range(max(x, 0), min(x + w, W)):
                row[xx] = c

    fill(0, 0, W, 6, BAND)
    if dark:
        fill(399, 52, 40, 8, MOON)
        fill(391, 60, 48, 32, MOON)
        fill(399, 92, 40, 8, MOON)
        fill(415, 52, 24, 32, SLAB[True])
        for a, b in ((62, 62), (225, 40), (313, 80), (459, 147)):
            fill(a, b, 4, 4, STAR)
    else:
        fill(392, 52, 48, 48, SUN)
        fill(384, 60, 64, 32, SUN)
        fill(400, 44, 32, 64, SUN)
        fill(400, 60, 32, 32, SUN_CORE)

    # The desk, on the art grid at 8x: its top face, a lit lip along its
    # front edge, then its front down to the cover's foot, with grain.
    d = DESK[dark]
    s = COVER_SCALE

    def cell(ax, ay, c):
        fill(ax * s, COVER_TOP + ay * s, s, s, c)

    last_row = (H - COVER_TOP) // s     # the grid row at the cover's foot
    for ay in range(DESK_BACK, last_row + 1):
        for ax in range(GRID):
            if ay < DESK_LIP:
                c = d["top"]
            elif ay == DESK_LIP:
                c = d["lip"]
            else:
                c = d["front"]
            cell(ax, ay, c)
    # Shadows of the cartridges on the desk's top face.
    x0 = CART_X - 1
    x1 = CART_X + CART_PITCH * (len(ROW) - 1) + len(SMALL_CART[0])
    for ax in range(x0, x1 + 1):
        cell(ax, CART_Y + len(SMALL_CART) + 1, d["shadow"])
    # The front's grain, short dashes as PortMaster's water has its ripples.
    for ax, ay, w in ((4, 58, 7), (22, 61, 8), (43, 59, 6), (52, 64, 7),
                      (9, 66, 9), (31, 68, 6), (45, 71, 8), (16, 73, 5)):
        for i in range(w):
            cell(ax + i, ay, d["grain"])

    g = drawing(with_shelf=False)
    for ay in range(GRID):
        for ax in range(GRID):
            c = g.px[ay][ax]
            if c:
                cell(ax, ay, c)

    rows = []
    for y in range(H):
        row = bytearray()
        for x in range(W):
            red, green, blue, _ = img[y][x]
            row += bytes((red, green, blue, corner_alpha(x, y)))
        rows.append(bytes(row))
    return rows


def main():
    os.makedirs(OUT, exist_ok=True)
    png(os.path.join(OUT, "icon.png"), GRID * ICON_SCALE, GRID * ICON_SCALE,
        icon())
    png(os.path.join(OUT, "cover-light.png"), COVER_W, COVER_H, cover(False))
    png(os.path.join(OUT, "cover-dark.png"), COVER_W, COVER_H, cover(True))


if __name__ == "__main__":
    main()
