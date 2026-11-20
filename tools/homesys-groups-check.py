#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A system turned off the home keeps its games on Favorites and Recents, and
they start from there (src/shelfbuild.c's hide_empty_systems, app.parked).

    make check-homesys-groups

2026-11-19, from the Discord (Davistoa: hide every system and live in
Favorites), my OK: MENU > Display > Home Systems hides a system's shelf and
nothing else. The real host launcher reads a card with games on the SNES, the
GBA and the Neo Geo Pocket Color, the SNES turned off (home.hidden), two
favorites and two games played, one of each on the SNES. Then:

- the home has no SNES shelf: ZYPEROS_AUTOLAUNCH finds no SFC game on it;
- Favorites and Recents each hold both games, the SNES one too, and look
  the same, pixel for pixel, in both themes, as on the same card with the SNES
  on the home: the same games, in the same order (Recents' is the play time's,
  the SNES game first);
- A on the SNES game on Favorites, and on Recents, starts it: Diatom (a
  stand-in here that writes what it was given) gets the SNES's core and the
  game's path in the SNES folder;
- powered off in the SNES game, the next start resumes it, on Recents,
  where the game played last is, since it has no shelf of its own;
- Delete Game on the SNES game on Favorites takes it off the card and off
  Favorites; on the last one, on Recents, the SNES goes from the systems off
  the home as an emptied shelf goes from the home;
- a collection naming the SNES game and a GBA one (DESIGN.md 11.56) keeps
  both, and its shelf, opened on it, looks the same with the SNES off as on;
- with Favorites, Recents and Collections off as well, nothing of the SNES
  is anywhere;
- Screenshots (DESIGN.md 11.68): with the SNES off, a shot of its game is on
  the Screenshots shelf with the others, the newest first, and A on it in the
  viewer plays the game on the SNES's core; a shot of a game gone from the
  card stays, with no game; Delete Screenshot takes its file, the last one
  the shelf; no shot, or the group turned off, no shelf.

Shots of Favorites and Recents in both themes go to
build-native/homesys-groups-shots/."""
import os
import shutil
import sqlite3
import subprocess
import time
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build-native/homesys-groups-shots'
BIN = Path(os.environ.get('ZYPEROS_BIN', ROOT / 'build-native/zyperos'))
STATUS = '10:08 24h wifi=3 bt batt=82'

GAMES = {'SFC': ('SNES', ['Chrono Trigger (USA).sfc', 'Super Metroid (USA).sfc']),
         'GBA': ('Game Boy Advance', ['Metroid Fusion (USA).gba', 'Golden Sun (USA).gba']),
         'NGPC': ('Neo Geo Pocket Color', ['Metal Slug - 2nd Mission (World) (En,Ja).ngc'])}
FAVS = [('SFC', 'Chrono Trigger (USA).sfc'), ('GBA', 'Metroid Fusion (USA).gba')]
# Played last first: the SNES game an hour ago, the GBA game two
PLAYED = [('SFC', 'Super Metroid (USA).sfc'), ('GBA', 'Golden Sun (USA).gba')]
# Diatom's stand-in: each start's arguments, one a line, and a line "--" after
# them. The launcher starts it for the game, and again after it as the resident
# for the next one (respawn_resident), which exits at once here.
DIATOM = '#!/bin/sh\nprintf "%s\\n" "$@" -- >> "$(dirname "$0")/diatom-args.txt"\nexit 0\n'


def card(name, hidden, coll=None):
    base = OUT / name
    if base.exists():
        shutil.rmtree(base)
    for d in ('root', 'Roms', 'userdata', 'shared'):
        (base / d).mkdir(parents=True)
    rows = []
    for line in (ROOT / 'config/systems.cfg').read_text().splitlines():
        f = line.split('|')
        if line.startswith('sys|') and f[4] in GAMES:
            rows.append(line)
            folder, games = GAMES[f[4]]
            (base / 'Roms' / folder).mkdir(parents=True, exist_ok=True)
            for g in games:
                (base / 'Roms' / folder / g).write_bytes(b'\0' * 4096)
    (base / 'root/systems.cfg').write_text('\n'.join(rows) + '\n')
    if coll:
        (base / 'Collections').mkdir()
        (base / 'Collections/Couch.txt').write_text(coll)
    (base / 'root/system-icons').symlink_to(ROOT / 'res/system-icons', target_is_directory=True)
    (base / 'root/diatom').write_text(DIATOM)
    (base / 'root/diatom').chmod(0o755)
    env = os.environ | {'ZYPEROS_HEADLESS': '1', 'SDL_VIDEODRIVER': 'dummy',
                        'SDL_RENDER_DRIVER': 'software', 'ZYPEROS_ROOT': str(base / 'root'),
                        'ZYPEROS_CARD': str(base), 'ZYPEROS_ROMS': str(base / 'Roms'),
                        'ZYPEROS_USERDATA': str(base / 'userdata'), 'ZYPEROS_SHARED': str(base / 'shared'),
                        'ZYPEROS_WEB': str(base), 'ZYPEROS_FONT': str(ROOT / 'res/fonts/menu.ttf')}
    env.pop('ZYPEROS_AUTOLAUNCH', None)
    run(env, '--shot', str(base / 'first.png'), '--screen', 'systems', '--status', STATUS)
    with sqlite3.connect(base / 'shared/.zyperos/library.db') as lib:
        for tag, game in FAVS:
            lib.execute('INSERT OR REPLACE INTO settings(key, value) VALUES (?, ?)', (f'fav.{tag}\t{game}', '1'))
        for i, (tag, game) in enumerate(PLAYED):
            when = 1763000000 - 3600 * (i + 1)
            lib.execute('INSERT OR REPLACE INTO settings(key, value) VALUES (?, ?)',
                        (f'sess.{when}.{900 + i}.{tag}\t{game}', '600\tquit'))
    with sqlite3.connect(base / 'userdata/zyperos.db') as dev:
        dev.execute('CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL)')
        dev.execute('INSERT OR REPLACE INTO settings(key, value) VALUES (?, ?)', ('home.hidden', hidden))
    return base, env


def run(env, *args):
    r = subprocess.run([str(BIN), *args], env=env, cwd=ROOT, capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, (args, r.stderr[-1500:])
    return r.stderr


def shot(base, env, name, shelf, theme):
    """The shelf's games screen, and what the start said"""
    run(env, '--set', f'startup_system={shelf}')
    return run(env, '--shot', str(OUT / f'{name}.png'), '--screen', 'games', '--phase', '3200',
               '--status', STATUS, '--theme', theme)


def games_on(log, shelf):
    """'scan: Favorites 2 games' -> 2; no line -> 0 (no shelf)"""
    for line in log.splitlines():
        if line.startswith('scan: ') and line[6:].split() [:1] == [shelf] and 'game' in line:
            return int(line.split()[2])
    return 0


def coll_games(log):
    """'scan: Collections 1 collection, 2 games' -> 2; none -> 0"""
    for line in log.splitlines():
        f = line.split()
        if f[:2] == ['scan:', 'Collections'] and len(f) > 4 and f[4].isdigit():
            return int(f[4])
    return 0


def start(base, env, shelf, game):
    """ZYPEROS_AUTOLAUNCH on `shelf`'s `game`: what Diatom was given for the
    game (its --key value pairs), or None when no game started"""
    args = base / 'root/diatom-args.txt'
    if args.exists():
        args.unlink()
    log = run(env | {'ZYPEROS_AUTOLAUNCH': f'{shelf}\t{game}'})
    if not args.exists():
        return None, log
    runs, cur = [], []
    for line in args.read_text().splitlines():
        if line == '--':
            runs.append(cur)
            cur = []
        else:
            cur.append(line)
    games = [r for r in runs if '--rom' in r]
    if not games:
        return None, log
    assert len(games) == 1, f'{len(games)} games started for one A'
    a = games[0]
    return {a[i]: a[i + 1] for i in range(len(a) - 1) if a[i].startswith('--')}, log


OUT.mkdir(parents=True, exist_ok=True)
# The same card with every system on the home: what the groups must look like
base, env = card('all-on', '')
for theme in ('light', 'dark'):
    for shelf in ('Favorites', 'Recents'):
        log = shot(base, env, f'{shelf.lower()}-all-on-{theme}', shelf, theme)
        assert games_on(log, shelf) == 2, f'{shelf} on the card with every system on:\n' + log[-800:]

base, env = card('snes-off', 'SFC')
core = next(l.split('|')[3] for l in (ROOT / 'config/systems.cfg').read_text().splitlines()
            if l.startswith('sys|') and l.split('|')[4] == 'SFC').split(',')[0]

got, log = start(base, env, 'SFC', 'Chrono Trigger (USA).sfc')
assert got is None and 'autolaunch: no SFC / Chrono Trigger (USA).sfc' in log, \
    'the SNES turned off is still a shelf of the home:\n' + log[-800:]
assert '1 kept for Favorites and Recents' in log, 'the SNES was not kept off the home:\n' + log[-800:]

for theme in ('light', 'dark'):
    log = shot(base, env, f'favorites-snes-off-{theme}', 'Favorites', theme)
    assert games_on(log, 'Favorites') == 2, 'Favorites lost the SNES game:\n' + log[-800:]
    assert 'fav: unresolved SFC' not in log, 'a SNES favorite did not resolve:\n' + log[-800:]
    log = shot(base, env, f'recents-snes-off-{theme}', 'Recents', theme)
    assert games_on(log, 'Recents') == 2, 'Recents lost the SNES game:\n' + log[-800:]
    for shelf in ('favorites', 'recents'):
        on = Image.open(OUT / f'{shelf}-all-on-{theme}.png').convert('RGB')
        off = Image.open(OUT / f'{shelf}-snes-off-{theme}.png').convert('RGB')
        box = ImageChops.difference(on, off).getbbox()
        assert box is None, f'{shelf} ({theme}) with the SNES off the home differs from it on, at {box}'

for shelf, game in (('FAV', 'Chrono Trigger (USA).sfc'), ('RECENT', 'Super Metroid (USA).sfc')):
    got, log = start(base, env, shelf, game)
    assert got, f'A on {game} on {shelf} started nothing:\n' + log[-800:]
    rom = base / 'Roms/SNES' / game
    assert got.get('--rom') == str(rom), f'{shelf}: Diatom was given {got.get("--rom")}, not {rom}'
    assert got.get('--core', '').endswith(f'/cores/{core}_libretro.so'), \
        f'{shelf}: Diatom was given the core {got.get("--core")}, not the SNES\'s {core}'
    if shelf == 'RECENT':
        state = Path(got['--state-on-exit'])
# The GBA game beside it, a system on the home, still starts from there too
got, log = start(base, env, 'FAV', 'Metroid Fusion (USA).gba')
assert got and got.get('--rom') == str(base / 'Roms/Game Boy Advance/Metroid Fusion (USA).gba'), log[-800:]

# Powered off in Super Metroid (its auto state, and .playing as a power-off
# leaves it), the shelf left on the Neo Geo Pocket Color: the next start, a
# real one with no shot, goes straight back into it, from Recents
state.parent.mkdir(parents=True, exist_ok=True)
state.write_bytes(b'state')
(base / 'root/.playing').write_text('SFC\nSuper Metroid (USA).sfc\noff\n')
(base / 'root/.last').write_text('NGPC\nMetal Slug - 2nd Mission (World) (En,Ja).ngc\n\n')
args = base / 'root/diatom-args.txt'
args.unlink(missing_ok=True)
p = subprocess.Popen([str(BIN)], env=env, cwd=ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(300):              # 30 s at most: the resume is the start's first thing
        if args.exists() and '--rom' in args.read_text():
            break
        time.sleep(0.1)
finally:
    p.kill()
    p.wait()
rom = [l for l in (args.read_text().splitlines() if args.exists() else [])]
rom = [rom[i + 1] for i in range(len(rom) - 1) if rom[i] == '--rom']
assert rom == [str(base / 'Roms/SNES/Super Metroid (USA).sfc')], f'the start resumed {rom}, not Super Metroid'
last = (base / 'root/.last').read_text().splitlines()
assert last[:2] == ['RECENT', 'Super Metroid (USA).sfc'], f'resumed on {last[:2]}, not on Recents'

# Delete Game from the groups, on a card of its own: the first SNES game from
# Favorites, then the last from Recents, which empties the SNES
base, env = card('snes-delete', 'SFC')
for shelf, tag, game, left in (('Favorites', 'FAV', 'Chrono Trigger (USA).sfc', '1 game left'),
                                ('Recents', 'RECENT', 'Super Metroid (USA).sfc', '0 games left')):
    (base / 'root/.last').write_text(f'{tag}\n{game}\n\n')
    run(env, '--set', f'startup_system={shelf}')
    log = run(env, '--shot', str(OUT / f'delete-{tag.lower()}.png'), '--screen', 'games',
              '--delete-now', '--status', STATUS, '--theme', 'light')
    assert 'delete: deleted' in log and f'delete: SFC {game} off its shelf, {left}' in log, \
        f'Delete Game on {game} on {shelf}:\n' + log[-800:]
    assert not (base / 'Roms/SNES' / game).exists(), f'{game} is still on the card'
assert 'delete: SFC has no games left, off the home' in log, log[-800:]
log = run(env, '--shot', str(OUT / 'delete-after.png'), '--screen', 'systems', '--status', STATUS)
assert games_on(log, 'Favorites') == 1 and games_on(log, 'Recents') == 1, log[-800:]

# A collection with the SNES game and a GBA one keeps both with the SNES off
# the home, and its shelf, opened on it, is the same pixel for pixel
COLL = '/Roms/SNES/Chrono Trigger (USA).sfc\n/Roms/Game Boy Advance/Metroid Fusion (USA).gba\n'
for name, hidden in (('all-on', ''), ('snes-off', 'SFC')):
    base, env = card(f'coll-{name}', hidden, COLL)
    run(env, '--set', 'startup_system=Collections')
    for theme in ('light', 'dark'):
        log = run(env, '--shot', str(OUT / f'collection-{name}-{theme}.png'), '--screen', 'games',
                  '--folder', 'Couch', '--phase', '3200', '--status', STATUS, '--theme', theme)
        assert coll_games(log) == 2, f'the collection with {hidden or "every system"} off has not both games:\n' + log[-800:]
for theme in ('light', 'dark'):
    on = Image.open(OUT / f'collection-all-on-{theme}.png').convert('RGB')
    off = Image.open(OUT / f'collection-snes-off-{theme}.png').convert('RGB')
    box = ImageChops.difference(on, off).getbbox()
    assert box is None, f'the collection ({theme}) with the SNES off the home differs from it on, at {box}'

# Favorites, Recents and Collections off as well: the SNES nowhere, the rest
# as it was
base, env = card('all-off', 'SFC,FAV,RECENT,COLL', COLL)
log = shot(base, env, 'home-groups-off', 'Game Boy Advance', 'light')
assert games_on(log, 'Favorites') == 0 and games_on(log, 'Recents') == 0, log[-800:]
assert coll_games(log) == 0, 'Collections is on the home with it off:\n' + log[-800:]
for shelf, game in (('FAV', 'Chrono Trigger (USA).sfc'), ('SFC', 'Chrono Trigger (USA).sfc')):
    got, log = start(base, env, shelf, game)
    assert got is None, f'{shelf} {game} started with the SNES and the groups off'

# Screenshots (DESIGN.md 11.68): three shots, as the hotkey names them, one of
# the SNES game, one of a GBA game and one of a game no longer on the card
SHOTS = ['SNES/Chrono Trigger (USA) 2026-11-20 09.12.40.png',
         'Game Boy Advance/Metroid Fusion (USA) 2026-11-19 22.05.11.png',
         'SNES/Gone Game (USA) 2026-11-18 21.40.05.png']


def shots_on(log):
    """'scan: Screenshots 3 screenshots' -> 3; no line -> 0 (no shelf)"""
    for line in log.splitlines():
        f = line.split()
        if f[:2] == ['scan:', 'Screenshots'] and len(f) > 2 and f[2].isdigit():
            return int(f[2])
    return 0


base, env = card('shots', 'SFC')
log = run(env, '--shot', str(OUT / 'shots-none.png'), '--screen', 'systems', '--status', STATUS)
assert shots_on(log) == 0, 'a Screenshots shelf with no shot on the card:\n' + log[-800:]
for k, f in enumerate(SHOTS):
    (base / 'Screenshots' / f).parent.mkdir(parents=True, exist_ok=True)
    Image.new('RGB', (256 if f.startswith('SNES') else 240, 224 if f.startswith('SNES') else 160),
              (40 * k, 90, 160)).save(base / 'Screenshots' / f)
(base / 'Screenshots/SNES/ScreenShot - 0000.png').write_bytes(b'not a shot of the hotkey')
run(env, '--set', 'startup_system=Screenshots')
log = run(env, '--shot', str(OUT / 'shots-shelf.png'), '--screen', 'games', '--shelf-press', 'a',
          '--status', STATUS)
assert shots_on(log) == 3, 'the SNES off the home, its shot not on Screenshots:\n' + log[-800:]
assert 'shelf-press: a -> top [Chrono Trigger (USA).sfc] 1/3' in log, \
    'Screenshots is not newest first, or A did not open its shot:\n' + log[-800:]
got, log = start(base, env, 'SHOT', 'Chrono Trigger (USA).sfc')
assert got and got.get('--rom') == str(base / 'Roms/SNES/Chrono Trigger (USA).sfc') and \
    got.get('--core', '').endswith(f'/cores/{core}_libretro.so'), \
    'A on the SNES game\'s shot did not start it on the SNES\'s core:\n' + log[-800:]
for n in (2, 1, 0):
    log = run(env, '--shot', str(OUT / f'shots-delete-{n}.png'), '--screen', 'games', '--shots-delete-now',
              '--status', STATUS)
    assert 'shots: deleted' in log, 'Delete Screenshot deleted nothing:\n' + log[-800:]
    assert len(list((base / 'Screenshots').rglob('* 2026-*.png'))) == n, f'{n} shots should be left'
log = run(env, '--shot', str(OUT / 'shots-gone.png'), '--screen', 'systems', '--status', STATUS)
assert shots_on(log) == 0 and (base / 'Screenshots/SNES/ScreenShot - 0000.png').exists(), \
    'the last shot deleted, Screenshots is still on the home, or another file went:\n' + log[-800:]
base, env = card('shots-off', 'SHOT')
(base / 'Screenshots' / SHOTS[0]).parent.mkdir(parents=True, exist_ok=True)
Image.new('RGB', (256, 224), (40, 90, 160)).save(base / 'Screenshots' / SHOTS[0])
log = run(env, '--shot', str(OUT / 'shots-off.png'), '--screen', 'systems', '--status', STATUS)
assert shots_on(log) == 0, 'Screenshots turned off is on the home:\n' + log[-800:]

print('ok: a system off the home keeps its games on Favorites, Recents and Collections, the groups as they were, '
      'and they start and resume there on its own core; off the home and off the groups, nowhere; its shots on '
      'Screenshots, played from there, deleted from there')
