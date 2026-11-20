#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-mp4views: START on MP4's shelf turns its View (DESIGN.md 11.67), on
the real launcher: desk mode in SDL's dummy window, as check-desk runs it, so
no window opens, on the README's media card, driven over its control socket.
SELECT opens MP4 from the home; START let go turns Covers to Grid to List and round (never Video: an album
has no snap), each kept as view.MUSE in the handheld's database; SELECT with
START, in either order, is the lock's pair and turns nothing, and its SELECT
closes nothing either. build-native/zyperos must be built (make native)."""
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


def module(name, path):
    loader = importlib.machinery.SourceFileLoader(name, str(path))
    spec = importlib.util.spec_from_loader(name, loader)
    mod = importlib.util.module_from_spec(spec)
    loader.exec_module(mod)
    return mod


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


def view(card):
    """MP4's shelf's View as the handheld's database keeps it"""
    with sqlite3.connect(card / 'userdata/zyperos.db') as db:
        row = db.execute("SELECT value FROM settings WHERE key='view.MUSE'").fetchone()
    return row[0] if row else 'covers'


def settle(sock):
    ask(sock, 'wait-frames 12')


def run(desk, rs, tmp, device):
    card = tmp / device
    rs.media_card(card)
    desk.card_sounds(card)
    env = desk.card_env(card, None)
    subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=env, check=True, stdout=subprocess.DEVNULL)
    sock = Path(tempfile.gettempdir()) / f'zyperos-mp4views-{os.getpid()}-{device}.sock'
    flags = tmp / f'{device}-flags'
    flags.mkdir()
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(sock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(tmp / f'{device}-ctl'))
    env.pop('ZYPEROS_HEADLESS', None)
    log = open(tmp / f'{device}.log', 'w')
    p = subprocess.Popen([str(BINARY), '--device', device], env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(200):
            if sock.exists() or p.poll() is not None:
                break
            time.sleep(0.05)
        check(sock.exists(), f'{device}: the control socket is up')
        if not sock.exists():
            print((tmp / f'{device}.log').read_text()[-2000:])
            return
        settle(sock)
        # SELECT opens MP4 from the home, as from anywhere
        ask(sock, 'tap select')
        settle(sock)
        check(view(card) == 'covers', f'{device}: MP4 opens on Covers, as it ships')
        for want in ('grid', 'list', 'covers', 'grid'):
            ask(sock, 'tap start')
            settle(sock)
            check(view(card) == want, f'{device}: START let go turns it to {want.title()} ({view(card)})')
        # The lock's pair, both at once and START first: no turn, and
        # MP4 still open (its SELECT closed nothing), so START turns again
        ask(sock, 'press select+start')
        settle(sock)
        ask(sock, 'release select+start')
        settle(sock)
        check(view(card) == 'grid', f'{device}: SELECT with START turns nothing ({view(card)})')
        ask(sock, 'press start')
        settle(sock)
        ask(sock, 'press select')
        settle(sock)
        ask(sock, 'release select')
        ask(sock, 'release start')
        settle(sock)
        check(view(card) == 'grid', f'{device}: START, then SELECT joining it, turns nothing ({view(card)})')
        ask(sock, 'tap start')
        settle(sock)
        check(view(card) == 'list', f'{device}: MP4 still open after the pair: START turns it on ({view(card)})')
        shot = tmp / f'{device}-list.png'
        check(ask(sock, f'shot {shot}').get('ok') and shot.is_file(), f'{device}: the List drawn')
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
        print('check-mp4views: build-native/zyperos missing (make native)')
        return 1
    desk = module('desk', ROOT / 'tools/desk')
    rs = module('readme_shots', ROOT / 'tools/readme-shots.py')
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-mp4views-'))
    try:
        for device in ('pixel2', 'brick'):
            print(f'{device}:')
            run(desk, rs, tmp, device)
    finally:
        if not fails:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print(f'check-mp4views: {len(fails)} failed (logs in {tmp})')
        return 1
    print('ok: START turns MP4\'s shelf Covers, Grid, List and round, and the lock\'s pair turns nothing')
    return 0


if __name__ == '__main__':
    sys.exit(main())
