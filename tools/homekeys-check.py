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


def tap(sock, key):
    """One press and no repeat, however slowly the launcher draws: desk
    mode's own tap (src/device/desk.c), down at once and let go by a timer's
    event 40 ms on. The let-go is read at a poll after the one that read the
    press, so the pass that read it sees the key down; and it comes long
    before the repeat's 300 ms (src/keyrepeat.h), so no pass sees it held
    that long. A press held instead for two drawn frames repeated on a slow
    runner (2026-11-21, CI: a tap of Down two groups on, Right three games)"""
    ask(sock, f'tap {key} 40')


def until(get, ok, secs=5.0):
    """get() again until ok(it) holds or `secs` pass (a busy computer draws
    late); the last one got"""
    end = time.time() + secs
    got = get()
    while not ok(got) and time.time() < end:
        time.sleep(0.1)
        got = get()
    return got


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


def hold(sock, button, done, limit=10.0, on_frames=12, on_ms=700):
    """`button` held until done(seen) is true or `limit` seconds, the cursor
    read as it goes; then held on through `on_frames` more frames and at
    least `on_ms` (a repeat is due every 90 ms after 300: several, on any
    computer, and as many frames as a slow one draws). Each cursor it showed,
    and how many of them there were when done() came true: any more came
    while it was held on past it. No wall clock decides where it ends, so a
    slow runner holds longer, not differently."""
    seen = [ask(sock, 'state')['cursor']]
    ask(sock, f'press {button}')

    def look():
        c = ask(sock, 'state')['cursor']
        if c != seen[-1]:
            seen.append(c)

    t0 = time.monotonic()
    while not done(seen) and time.monotonic() - t0 < limit:
        look()
        time.sleep(0.01)
    reached = len(seen) if done(seen) else -1
    t1 = time.monotonic()
    if on_frames > 0:
        ask(sock, f'wait-frames {on_frames}', timeout=30)
    while time.monotonic() - t1 < on_ms / 1000:
        look()
        time.sleep(0.01)
    look()
    ask(sock, f'release {button}')
    time.sleep(0.15)
    look()
    return seen, reached


def settle(sock):
    # A slow runner draws 6 frames in more than the socket's usual 10 s
    ask(sock, 'wait-frames 6', timeout=60)
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

        seen, at = hold(sock, 'left', lambda v: v[-1] == 0)
        check(at > 0 and seen[-1] == 0 and all(b - a == -1 for a, b in zip(seen, seen[1:])),
              f'{n} systems, Grid: a held Left walks to the first system and stops there ({seen})')

        seen, at = hold(sock, 'down', lambda v: v[-1] == last_row)
        steps = [b - a for a, b in zip(seen, seen[1:])]
        check(at > 0 and all(s == cols for s in steps[:at - 1]),
              f'{n} systems, Grid: a held Down walks a row at a time to the last row ({seen})')
        check(at > 0 and len(seen) == at, f'{n} systems, Grid: held on at the last row, Down stays there ({seen})')

        seen, at = hold(sock, 'up', lambda v: v[-1] == 0)
        steps = [b - a for a, b in zip(seen, seen[1:])]
        check(at > 0 and all(s == -cols for s in steps[:at - 1]),
              f'{n} systems, Grid: a held Up walks a row at a time to the first row ({seen})')
        check(at > 0 and len(seen) == at, f'{n} systems, Grid: held on at the first row, Up stays there ({seen})')

        # START: the Grid's next layout, Spotlight. Its Down jumps a group
        # (three by release date, an initial A to Z) and goes round the end,
        # so the held walk is held to Down tapped as often from the same
        # place: three repeats after the press, however long they take
        tap(sock, 'start')
        settle(sock)
        held, _ = hold(sock, 'down', lambda v: len(v) >= 4, on_frames=0, on_ms=0)
        for layout in ('gallery', 'grid', 'spotlight'):
            tap(sock, 'start')
            settle(sock)
            if layout == 'grid':
                hold(sock, 'left', lambda v: v[-1] == 0, on_frames=0, on_ms=0)   # back to the first system
        tapped = [ask(sock, 'state')['cursor']]
        for _ in range(len(held) - 1):
            tap(sock, 'down')
            ask(sock, 'wait-frames 4', timeout=30)
            # Down jumps a group or goes round: the cursor moves, late on a
            # busy computer
            tapped.append(until(lambda: ask(sock, 'state')['cursor'], lambda c: c != tapped[-1], 3.0))
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
