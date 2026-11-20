#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-homekeys: the home's d-pad held down, as a thumb holds it, in desk
mode (src/device/desk.h) in SDL's dummy window, driven over its control
socket, its `state` read for where the cursor landed after every frame.

On the Grid (DESIGN.md 11.1) a held Down walks a row at a time to the last
row and stops there, a held Up back to the first and stops there, the
window scrolling with it on a card of more systems than three rows show; a
held Left stops at the first system. On Spotlight a held Down jumps
forward group by group and never back.

Each step of a held direction once read the button twice in a frame
(`in_repeat(UP) || in_repeat(DOWN)`, then `in_repeat(DOWN) ? 1 : -1`), and
a repeat's second read is false: a held Down went down once and then back
up, row by row, on the Grid and on Spotlight both (2026-11-19, on the 1.1.0
test build: "pressing and hold up/down is not behaving as expected").
build-native/zyperos must be built (make native)."""
import importlib.machinery
import importlib.util
import json
import os
import shutil
import socket
import sqlite3
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build-native/zyperos'
fails = []


def check(ok, what):
    print(f'  {"ok  " if ok else "FAIL"} {what}')
    if not ok:
        fails.append(what)


def ask(sock, line, timeout=10):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.settimeout(timeout)
    s.connect(str(sock))
    s.sendall((line + '\n').encode())
    s.shutdown(socket.SHUT_WR)
    out = b''
    while True:
        b = s.recv(4096)
        if not b:
            break
        out += b
    return json.loads(out.decode() or '{}')


def card(base, n):
    """A card of the first `n` systems of config/systems.cfg, a game each"""
    for d in ('ZyperOS', 'Roms', 'userdata', 'shared', 'web'):
        (base / d).mkdir(parents=True, exist_ok=True)
    rows = [l for l in (ROOT / 'config/systems.cfg').read_text().splitlines() if l.startswith('sys|')][:n]
    for line in rows:
        f = line.split('|')
        folder = base / 'Roms' / f[2]
        folder.mkdir(parents=True, exist_ok=True)
        (folder / ('Example.' + f[7].split(',')[0])).touch()
    (base / 'ZyperOS/systems.cfg').write_text('\n'.join(rows) + '\n')
    (base / 'ZyperOS/system-icons').symlink_to(ROOT / 'res/system-icons', target_is_directory=True)
    env = dict(os.environ, ZYPEROS_ROOT=str(base / 'ZyperOS'), ZYPEROS_CARD=str(base),
               ZYPEROS_ROMS=str(base / 'Roms'), ZYPEROS_USERDATA=str(base / 'userdata'),
               ZYPEROS_SHARED=str(base / 'shared'), ZYPEROS_WEB=str(base / 'web'),
               ZYPEROS_FONT=str(ROOT / 'res/fonts/menu.ttf'))
    for kv in ('tour.done=1', 'home=grid'):
        subprocess.run([str(BINARY), '--set', kv], env=env, check=True, stdout=subprocess.DEVNULL)
    return env


def hold(sock, button, ms):
    """`button` held `ms`, the cursor read as it goes: each one it showed"""
    seen = [ask(sock, 'state')['cursor']]
    ask(sock, f'press {button}')
    end = time.monotonic() + ms / 1000
    while time.monotonic() < end:
        c = ask(sock, 'state')['cursor']
        if c != seen[-1]:
            seen.append(c)
        time.sleep(0.012)
    ask(sock, f'release {button}')
    time.sleep(0.15)
    c = ask(sock, 'state')['cursor']
    if c != seen[-1]:
        seen.append(c)
    return seen


def settle(sock):
    ask(sock, 'wait-frames 6')
    time.sleep(0.1)


def run(tmp, n, cols):
    base = tmp / f'card-{n}'
    env = card(base, n)
    sock = Path(tempfile.gettempdir()) / f'zyperos-homekeys-{os.getpid()}-{n}.sock'
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(sock), ZYPEROS_FLAG_DIR=str(tmp / f'flags-{n}'),
               ZYPEROS_CTL_DIR=str(tmp / f'ctl-{n}'))
    (tmp / f'flags-{n}').mkdir()
    env.pop('ZYPEROS_HEADLESS', None)
    log = open(tmp / f'{n}.log', 'w')
    p = subprocess.Popen([str(BINARY), '--device', 'pixel2'], env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(200):
            if sock.exists() or p.poll() is not None:
                break
            time.sleep(0.05)
        check(sock.exists(), f'{n} systems: the control socket is up')
        if not sock.exists():
            print((tmp / f'{n}.log').read_text()[-2000:])
            return
        for _ in range(100):
            if ask(sock, 'state').get('screen') == 'systems':
                break
            time.sleep(0.05)
        rows = (n + cols - 1) // cols
        last_row = (rows - 1) * cols
        settle(sock)

        seen = hold(sock, 'left', 2500)
        check(seen[-1] == 0, f'{n} systems, Grid: a held Left stops at the first system ({seen})')

        seen = hold(sock, 'down', 300 + 150 * (rows + 2))
        steps = [b - a for a, b in zip(seen, seen[1:])]
        check(all(s == cols for s in steps) and seen[-1] == last_row,
              f'{n} systems, Grid: a held Down walks a row at a time to the last row ({seen})')
        seen = hold(sock, 'down', 700)
        check(seen == [last_row], f'{n} systems, Grid: held at the last row, Down stays there ({seen})')

        seen = hold(sock, 'up', 300 + 150 * (rows + 2))
        steps = [b - a for a, b in zip(seen, seen[1:])]
        check(all(s == -cols for s in steps) and seen[-1] == 0,
              f'{n} systems, Grid: a held Up walks a row at a time to the first row ({seen})')
        seen = hold(sock, 'up', 700)
        check(seen == [0], f'{n} systems, Grid: held at the first row, Up stays there ({seen})')

        # START: the Grid's next layout, Spotlight. Its Down jumps a group
        # (three by release date, an initial A to Z) and goes round the end,
        # so the held walk is held to Down tapped as often from the same place
        ask(sock, 'tap start')
        settle(sock)
        held = hold(sock, 'down', 300 + 90 * 3)
        for layout in ('gallery', 'grid', 'spotlight'):
            ask(sock, 'tap start')
            settle(sock)
            if layout == 'grid':
                hold(sock, 'left', 2500)        # back to the first system
        tapped = [ask(sock, 'state')['cursor']]
        for _ in range(len(held) - 1):
            ask(sock, 'tap down 40')
            time.sleep(0.25)
            tapped.append(ask(sock, 'state')['cursor'])
        check(len(held) >= 3 and held == tapped,
              f'{n} systems, Spotlight: a held Down jumps forward as taps of it do (held {held}, tapped {tapped})')
        ask(sock, 'quit')
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            pass
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
        log.close()
        if sock.exists():
            sock.unlink()


def main():
    if not BINARY.is_file():
        print('check-homekeys: build-native/zyperos missing (make native)')
        return 1
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-homekeys-'))
    try:
        # 24: the full grid, six across, four rows, three in sight (it
        # scrolls); 8: two rows of four
        run(tmp, 24, 6)
        run(tmp, 8, 4)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'homekeys: {"all passed" if not fails else f"{len(fails)} failed"}')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
