#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The card of a game with no art (src/cardart.h), held on the launcher's own
shots:

1. Every game with no art gets the card: Zeta Run on Genesis and DuckTales
   on NES (the two that started this, 2026-11-15), and a long name on
   PlayStation, never played, in its system's ground.
2. A played game shows its frame of play: <rom>.card.bmp, Diatom's (test/
   cardframe.py there), and until there is one the Auto slot's
   <rom>.auto.bmp. The card frame wins when both are there.
3. The card is the same in both themes, as a box is.
4. The names of a Grid sit on one reading line: every card's first line at
   the same height in its cell.
5. As the cover row stops on such a card (src/focusfx.h): one light across
   it that lights nothing round it, and a played game's frame turning sharp;
   neither on a cover with art, and nothing of either on a still shelf
   (--sheen, --sharp: the shelf at a point of them).

The frames are flat colors, so what the card shows is told by its hue: green
for the frame of play, against Genesis's blue ground and NES's red one.

    make check-cardart-shots     (builds the host launcher; needs Pillow)

The shots go to build-native/cardart-shots (CARDART_SHOTS_OUT to move them)."""
import os
import shutil
import subprocess
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ.get('CARDART_SHOTS_OUT', ROOT / 'build-native/cardart-shots'))
BIN = ROOT / 'build-native/zyperos'
STATUS = '10:08 24h wifi=off batt=82'
GAMES = {'Genesis': ['Zeta Run (Homebrew).md', 'Altered Beast (USA, Europe).md',
                     'Sonic The Hedgehog 2 (USA).md', 'Comix Zone (USA).md'],
         'NES': ['DuckTales (USA).nes', 'Mega Man 2 (USA).nes'],
         'PlayStation': ['Castlevania - Symphony of the Night (USA).chd']}
GREEN, MAGENTA = (40, 230, 60), (230, 40, 220)
# The focused card on the cover row (1024 x 768): its top, above the band
CARD_TOP = (404, 224, 620, 300)
FAILED = []


def need(cond, why):
    if not cond:
        FAILED.append(why)
        print('  FAIL', why)


def fixture():
    base = OUT / 'card'
    if base.exists():
        shutil.rmtree(base)
    for d in ('root', 'userdata', 'shared/.zyperos'):
        (base / d).mkdir(parents=True)
    for folder, games in GAMES.items():
        (base / 'Roms' / folder).mkdir(parents=True)
        for g in games:
            (base / 'Roms' / folder / g).write_bytes(b'x')
    rows = [l for l in (ROOT / 'config/systems.cfg').read_text().splitlines()
            if l.startswith('sys|') and l.split('|')[4] in ('MD', 'NES', 'PS1')]
    (base / 'root/systems.cfg').write_text('\n'.join(rows) + '\n')
    (base / 'root/system-icons').symlink_to(ROOT / 'res/system-icons', target_is_directory=True)
    # one game with box art, a plain red box, for what art never gets
    (base / 'Roms/Genesis/.media').mkdir()
    Image.new('RGB', (300, 420), (200, 40, 40)).save(base / 'Roms/Genesis/.media/Comix Zone (USA).png')
    # played: Sonic 2 has both pictures, the card frame green; Mega Man 2 only
    # the Auto slot's, green too
    for folder, rom, kind, color in (('Genesis', 'Sonic The Hedgehog 2 (USA).md', 'card', GREEN),
                                     ('Genesis', 'Sonic The Hedgehog 2 (USA).md', 'auto', MAGENTA),
                                     ('NES', 'Mega Man 2 (USA).nes', 'auto', GREEN)):
        d = base / 'shared/.zyperos' / folder
        d.mkdir(parents=True, exist_ok=True)
        # in stripes of its color and a darker one, so blurred and sharp
        # differ, and its hue stays its color
        im = Image.new('RGB', (320, 224), color)
        dark = tuple(c // 2 for c in color)
        for x in range(0, 320, 16):
            im.paste(dark, (x, 0, x + 8, 224))
        im.save(d / f'{rom}.{kind}.bmp')
    env = os.environ | {'ZYPEROS_HEADLESS': '1', 'SDL_VIDEODRIVER': 'dummy',
                        'SDL_RENDER_DRIVER': 'software', 'SDL_AUDIODRIVER': 'dummy',
                        'ZYPEROS_ROOT': str(base / 'root'), 'ZYPEROS_CARD': str(base),
                        'ZYPEROS_ROMS': str(base / 'Roms'), 'ZYPEROS_USERDATA': str(base / 'userdata'),
                        'ZYPEROS_SHARED': str(base / 'shared'), 'ZYPEROS_WEB': str(base),
                        'ZYPEROS_FONT': str(ROOT / 'res/fonts/menu.ttf')}
    return base, env


def shot(base, env, name, tag, focus, theme, *flags):
    (base / 'root/.last').write_text(f'{tag}\n{focus}\n')
    f = OUT / f'{name}-{theme}.png'
    r = subprocess.run([str(BIN), '--shot', str(f), '--theme', theme, '--screen', 'systems',
                        '--phase', '3200', '--status', STATUS, '--screen', 'games', *flags],
                       env=env, cwd=ROOT, capture_output=True, text=True)
    assert r.returncode == 0 and f.exists(), (name, r.stdout[-400:], r.stderr[-1200:])
    return Image.open(f).convert('RGB')


def mean(im, box):
    px = list(im.crop(box).getdata())
    return tuple(sum(p[k] for p in px) / len(px) for k in range(3))


def first_lines(im, y0, y1, lo=232):
    """The first near-white row of each run of near-white columns: one per
    Grid cell holding a drawn name. On Salt & Pepper `lo` is lower: the
    cards out of focus are drawn a fifth of the way toward its dark ground
    (Highlight's step back, DESIGN.md 11.47), the names' white 207"""
    w = im.size[0]
    px = im.load()
    cols = {}
    for x in range(32, w - 32):
        for y in range(y0, y1):
            r, g, b = px[x, y]
            if r > lo and g > lo and b > lo - 7:
                cols[x] = y
                break
    names, run = [], []
    for x in sorted(cols):
        if run and x - run[-1] > 24:
            names.append(min(cols[c] for c in run))
            run = []
        run.append(x)
    if run:
        names.append(min(cols[c] for c in run))
    return names


OUT.mkdir(parents=True, exist_ok=True)
base, env = fixture()
for theme in ('dark', 'light'):
    zeta = shot(base, env, 'zeta', 'MD', 'Zeta Run (Homebrew).md', theme)
    sonic = shot(base, env, 'sonic2', 'MD', 'Sonic The Hedgehog 2 (USA).md', theme)
    duck = shot(base, env, 'ducktales', 'NES', 'DuckTales (USA).nes', theme)
    mega = shot(base, env, 'megaman2', 'NES', 'Mega Man 2 (USA).nes', theme)
    shot(base, env, 'castlevania', 'PS1', 'Castlevania - Symphony of the Night (USA).chd', theme)
    grid = shot(base, env, 'grid', 'MD', 'Zeta Run (Homebrew).md', theme, '--view', 'grid')
    shot(base, env, 'list', 'NES', 'DuckTales (USA).nes', theme, '--view', 'list')

    z, s, d, m = (mean(i, CARD_TOP) for i in (zeta, sonic, duck, mega))
    need(z[2] > z[0] + 15 and z[2] > z[1], f'{theme}: Zeta Run never played: a card in Genesis blue {z}')
    need(d[0] > d[2] + 15 and d[0] > d[1], f'{theme}: DuckTales never played: a card in NES red {d}')
    need(s[1] > s[0] + 15 and s[1] > s[2] + 15, f'{theme}: Sonic 2 played: its card frame, green {s}')
    need(not (s[0] > s[1] and s[2] > s[1]), f'{theme}: the card frame wins over the Auto picture {s}')
    need(m[1] > m[0] + 15 and m[1] > m[2] + 15, f'{theme}: Mega Man 2: the Auto picture until a card frame {m}')

    lines = first_lines(grid, 90, 330, 198 if theme == 'dark' else 232)
    need(len(lines) >= 3, f'{theme}: the Grid shows its three names {lines}')
    need(lines and max(lines) - min(lines) <= 2, f'{theme}: the Grid\'s names on one reading line {lines}')

# 5. The row stopped on the card: the light, and the frame turning sharp
def stat(im, box):
    px = list(im.crop(box).getdata())
    lum = [.3 * p[0] + .59 * p[1] + .11 * p[2] for p in px]
    m = sum(lum) / len(lum)
    return m, sum((v - m) ** 2 for v in lum) / len(lum)

def edges(im, box):
    g = im.crop(box).convert('L')
    w, h = g.size
    p = g.load()
    return sum(abs(p[x + 1, y] - p[x, y]) for y in range(h) for x in range(w - 1)) / (w * h)

CARD_MID = (430, 250, 590, 520)      # inside the focused card
# Round the card (384-639 x 212-539 on the 1024 x 768 shot): just outside
# its rect at its four corners, and inside the rect where its round corners
# leave the shelf showing
ROUND = [(370, 198, 384, 212), (640, 198, 654, 212), (370, 540, 384, 554), (640, 540, 654, 554),
         (384, 212, 388, 215), (636, 212, 640, 215), (384, 537, 388, 540), (636, 537, 640, 540)]
still = shot(base, env, 'fx-still', 'MD', 'Zeta Run (Homebrew).md', 'dark')
lit = shot(base, env, 'fx-sheen', 'MD', 'Zeta Run (Homebrew).md', 'dark', '--sheen', '0.5')
need(stat(lit, CARD_MID)[0] > stat(still, CARD_MID)[0] + 2, 'the sheen lights the card')
for b in ROUND:
    need(abs(stat(lit, b)[0] - stat(still, b)[0]) < 1.5, f'the sheen lights nothing round the card {b}')
blurred = shot(base, env, 'fx-sharp0', 'MD', 'Sonic The Hedgehog 2 (USA).md', 'dark', '--sharp', '0')
sharp = shot(base, env, 'fx-sharp1', 'MD', 'Sonic The Hedgehog 2 (USA).md', 'dark', '--sharp', '1')
need(edges(sharp, CARD_MID) > edges(blurred, CARD_MID) * 1.2, 'a played card turns sharp')
art = shot(base, env, 'fx-art', 'MD', 'Comix Zone (USA).md', 'dark')
art_fx = shot(base, env, 'fx-art-sheen', 'MD', 'Comix Zone (USA).md', 'dark', '--sheen', '0.5', '--sharp', '1')
need(list(art.crop(CARD_MID).getdata()) == list(art_fx.crop(CARD_MID).getdata()), 'a cover with art gets neither')
again = shot(base, env, 'fx-still2', 'MD', 'Zeta Run (Homebrew).md', 'dark')
need(list(still.getdata()) == list(again.getdata()), 'a still shelf: nothing of either, the same frame twice')

# The same card in both themes, as a box is
for name in ('zeta', 'sonic2', 'ducktales', 'castlevania'):
    a = Image.open(OUT / f'{name}-dark.png').convert('RGB').crop((420, 240, 600, 520))
    b = Image.open(OUT / f'{name}-light.png').convert('RGB').crop((420, 240, 600, 520))
    diff = sum(abs(p - q) for pa, pb in zip(a.getdata(), b.getdata()) for p, q in zip(pa, pb))
    need(diff / (a.size[0] * a.size[1] * 3) < 3, f'{name}: the same card in both themes ({diff / (a.size[0] * a.size[1] * 3):.1f})')

if FAILED:
    raise SystemExit(f'cardart-shots: {len(FAILED)} FAILED')
print('cardart-shots: ok (no-art cards per system, frames of play, both themes, one reading line, the sheen and the sharp frame)')
