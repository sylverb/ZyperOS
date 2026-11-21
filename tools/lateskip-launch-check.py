#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A late frame left undrawn reaches Diatom on both ways a game starts.

    make check-lateskip-launch

A game normally starts in the resident Diatom with RUN, whose keys
check-config holds (cfg_run_keys: frameskip=late for a DS game's two screens
and for a system with a frameskip|tag|late line, the PSP's). When there is
no resident Diatom - the first game after it ended, or one that stopped
answering - the launcher starts a Diatom of its own for the game with the same
facts as arguments (src/launch_screen.c), and on 1.1.0-rc.6 that left out the
PSP's: its first game after the resident Diatom ended drew every frame
(2026-11-20). The real host launcher, a card with a PSP, a DS and a Game Boy
game, and a stand-in Diatom that writes what it was given (as
check-homesys-groups' does): each game started by ZYPEROS_AUTOLAUNCH, with no
resident Diatom to connect to.

- the PSP game gets --frameskip late, and not --screens;
- the DS game --screens 2 and --frameskip late, as before;
- the Game Boy game no --frameskip at all."""
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build-native/lateskip-launch'
BIN = Path(os.environ.get('ZYPEROS_BIN', ROOT / 'build-native/zyperos'))
GAMES = {'PSP': ('PSP', 'Tekken 6 (USA).iso'),
         'NDS': ('Nintendo DS', 'Mario Kart DS (USA).nds'),
         'GB': ('Game Boy', 'Tetris (World).gb')}
DIATOM = '#!/bin/sh\nprintf "%s\\n" "$@" -- >> "$(dirname "$0")/diatom-args.txt"\nexit 0\n'
fails = []


def check(name, ok, got=None):
    print(('  ok   ' if ok else '  FAIL ') + name + ('' if ok else f'\n         got  {got}'))
    if not ok:
        fails.append(name)


def card():
    if OUT.exists():
        shutil.rmtree(OUT)
    for d in ('root', 'Roms', 'userdata', 'shared'):
        (OUT / d).mkdir(parents=True)
    rows = []
    for line in (ROOT / 'config/systems.cfg').read_text().splitlines():
        f = line.split('|')
        if line.startswith('sys|') and f[4] in GAMES:
            rows.append(line)
            folder, game = GAMES[f[4]]
            (OUT / 'Roms' / folder).mkdir(parents=True, exist_ok=True)
            (OUT / 'Roms' / folder / game).write_bytes(b'\0' * 4096)
        elif not line.startswith('sys|') and len(f) >= 3 and f[1] in GAMES:
            rows.append(line)        # the PSP's needs and frameskip lines, the DS's pace...
    (OUT / 'root/systems.cfg').write_text('\n'.join(rows) + '\n')
    (OUT / 'root/system-icons').symlink_to(ROOT / 'res/system-icons', target_is_directory=True)
    (OUT / 'root/diatom').write_text(DIATOM)
    (OUT / 'root/diatom').chmod(0o755)
    env = os.environ | {'ZYPEROS_HEADLESS': '1', 'SDL_VIDEODRIVER': 'dummy',
                        'SDL_RENDER_DRIVER': 'software', 'ZYPEROS_ROOT': str(OUT / 'root'),
                        'ZYPEROS_CARD': str(OUT), 'ZYPEROS_ROMS': str(OUT / 'Roms'),
                        'ZYPEROS_USERDATA': str(OUT / 'userdata'), 'ZYPEROS_SHARED': str(OUT / 'shared'),
                        'ZYPEROS_WEB': str(OUT), 'ZYPEROS_FONT': str(ROOT / 'res/fonts/menu.ttf'),
                        'ZYPEROS_DIATOM_SOCKET': str(OUT / 'none.sock')}
    env.pop('ZYPEROS_AUTOLAUNCH', None)
    return env


def start(env, tag):
    """ZYPEROS_AUTOLAUNCH on `tag`'s game: the arguments Diatom got for it, as
    a list, or None when no game started"""
    args = OUT / 'root/diatom-args.txt'
    if args.exists():
        args.unlink()
    r = subprocess.run([str(BIN)], env=env | {'ZYPEROS_AUTOLAUNCH': f'{tag}\t{GAMES[tag][1]}'},
                       cwd=ROOT, capture_output=True, text=True, timeout=60)
    if not args.exists():
        return None, r.stderr
    runs, cur = [], []
    for line in args.read_text().splitlines():
        if line == '--':
            runs.append(cur)
            cur = []
        else:
            cur.append(line)
    games = [a for a in runs if '--rom' in a]
    return (games[0] if len(games) == 1 else None), r.stderr


def has(args, key, value=None):
    if not args or key not in args:
        return False
    i = args.index(key)
    return value is None or (i + 1 < len(args) and args[i + 1] == value)


if not BIN.exists():
    print(f'check-lateskip-launch: no {BIN} (make native)')
    raise SystemExit(1)
env = card()
psp, log = start(env, 'PSP')
check('the PSP game started, in a Diatom of its own', psp is not None, log[-600:])
check('the PSP game: --frameskip late', has(psp, '--frameskip', 'late'), psp)
check('and one screen (no --screens)', psp is not None and not has(psp, '--screens'), psp)
nds, log = start(env, 'NDS')
check('the DS game started', nds is not None, log[-600:])
check('the DS game: --screens 2 and --frameskip late, as before',
      has(nds, '--screens', '2') and has(nds, '--frameskip', 'late') and nds.count('--frameskip') == 1, nds)
gb, log = start(env, 'GB')
check('the Game Boy game started', gb is not None, log[-600:])
check('the Game Boy game: no --frameskip', gb is not None and not has(gb, '--frameskip'), gb)
if fails:
    print(f'check-lateskip-launch: {len(fails)} FAILED')
    raise SystemExit(1)
print('check-lateskip-launch: ok')
