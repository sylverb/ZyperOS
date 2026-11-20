#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-videoview: the Video view (src/videoview.h, DESIGN.md 11.57) as a
player meets it, in desk mode (src/device/desk.h) in SDL's dummy window,
over its control socket: a shelf of eight games, MENU > View > Video, its
snap playing (`--snap 1`, the test picture).

The snap playing holds the screensaver (DESIGN.md 11.37): past its delay
with nothing pressed, Right still steps to the next game. The screensaver
took the screen over the view with videos of its own, and ate the press
that ended it (2026-11-19, on the 1.1.0 test build: on full-screen video
every key "just expands the collapsed bar"). The covers' glide is
check-games' (vv_draw's frames), the snap held under a page check-snapshelf's.
build-native/zyperos must be built (make native)."""
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
GAMES = ['Alpha', 'Bravo', 'Charlie', 'Delta', 'Echo', 'Foxtrot', 'Golf', 'Hotel']
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


def run(tmp, name, snaps, saver, extra):
    base = tmp / name
    for d in ('ZyperOS', 'Roms', 'userdata', 'shared', 'web'):
        (base / d).mkdir(parents=True)
    sfc = [l for l in (ROOT / 'config/systems.cfg').read_text().splitlines()
           if l.startswith('sys|') and l.split('|')[4] == 'SFC']
    (base / 'ZyperOS/systems.cfg').write_text(sfc[0] + '\n')
    (base / 'ZyperOS/system-icons').symlink_to(ROOT / 'res/system-icons', target_is_directory=True)
    folder = base / 'Roms' / sfc[0].split('|')[2]
    (folder / '.media').mkdir(parents=True)
    for g in GAMES:
        (folder / f'{g}.sfc').touch()
        if snaps:
            (folder / '.media' / f'{g}.mp4').write_bytes(b'\0' * 64)
    env = dict(os.environ, ZYPEROS_ROOT=str(base / 'ZyperOS'), ZYPEROS_CARD=str(base),
               ZYPEROS_ROMS=str(base / 'Roms'), ZYPEROS_USERDATA=str(base / 'userdata'),
               ZYPEROS_SHARED=str(base / 'shared'), ZYPEROS_WEB=str(base / 'web'),
               ZYPEROS_FONT=str(ROOT / 'res/fonts/menu.ttf'))
    subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=env, check=True, stdout=subprocess.DEVNULL)
    with sqlite3.connect(base / 'userdata/zyperos.db') as db:
        db.execute("insert or replace into settings(key,value) values('view.SFC','video')")
        db.execute("insert or replace into settings(key,value) values('saver',?)", (str(saver),))
    (base / 'ZyperOS/.last').write_text('SFC\nAlpha.sfc\n\n')
    sock = Path(tempfile.gettempdir()) / f'zyperos-vvcheck-{os.getpid()}-{name}.sock'
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(sock), ZYPEROS_FLAG_DIR=str(tmp / f'{name}-flags'),
               ZYPEROS_CTL_DIR=str(tmp / f'{name}-ctl'))
    (tmp / f'{name}-flags').mkdir()
    env.pop('ZYPEROS_HEADLESS', None)
    log = open(tmp / f'{name}.log', 'w')
    p = subprocess.Popen([str(BINARY), '--device', 'brick', *extra], env=env, stdout=log,
                         stderr=subprocess.STDOUT)
    try:
        for _ in range(200):
            if sock.exists() or p.poll() is not None:
                break
            time.sleep(0.05)
        check(sock.exists(), f'{name}: the control socket is up')
        if not sock.exists():
            print((tmp / f'{name}.log').read_text()[-2000:])
            return None
        time.sleep(0.5)
        ask(sock, 'tap a')
        for _ in range(100):
            if ask(sock, 'state').get('screen') == 'games':
                break
            time.sleep(0.05)
        return sock, p, log, tmp / f'{name}.log'
    except Exception:
        p.kill()
        raise


def stop(sock, p, log):
    try:
        ask(sock, 'quit')
        p.wait(timeout=10)
    except Exception:
        pass
    if p.poll() is None:
        p.kill()
        p.wait()
    log.close()
    if sock.exists():
        sock.unlink()


def saver(tmp):
    got = run(tmp, 'saver', True, 3, ['--snap', '1'])
    if not got:
        return
    sock, p, log, logfile = got
    try:
        time.sleep(6.0)                   # twice the screensaver's delay, nothing pressed
        c0 = ask(sock, 'state')['cursor']
        ask(sock, 'tap right 40')
        time.sleep(0.6)
        c1 = ask(sock, 'state')['cursor']
        started = 'saver: starts' in logfile.read_text()
        check(not started and c1 == (c0 + 1) % len(GAMES),
              f'the snap playing holds the screensaver: Right steps on past its delay '
              f'(cursor {c0} -> {c1}; the screensaver {"started" if started else "did not start"})')
    finally:
        stop(sock, p, log)


def main():
    if not BINARY.is_file():
        print('check-videoview: build-native/zyperos missing (make native)')
        return 1
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-vvcheck-'))
    try:
        saver(tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'videoview: {"all passed" if not fails else f"{len(fails)} failed"}')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
