#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-mp4resume's start: MP4 back after a power-off (DESIGN.md 11.67) on
the real launcher, in desk mode's dummy window as check-desk runs it, so no
window opens and no sound plays (the host has no daemon: what Now Playing
shows is the launcher's own, before a daemon would answer). On the README's
media card, the row the launcher keeps (mp4.resume) set as a power-off left
it, then a start:
  - playing, a song: Now Playing opens by itself on that song at its second,
    paused, and the row says paused now; B leaves to the shelf; the next
    start opens nothing (it was not heard);
  - playing, a book and a video: the same, each on its own Now Playing;
  - paused: nothing opens; a file no longer on the card: nothing, and the
    row is gone.
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
KEY = 'mp4.resume'
SONG = 'Music/Demo Library/First Light/02 Open Road.mp3'
BOOK = 'Audiobooks/Treasure Island/01 The Old Sea-dog at the Admiral Benbow.mp3'
VIDEO = 'Videos/Sintel.mp4'
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


def row(card, value=None, set_it=False):
    with sqlite3.connect(card / 'userdata/zyperos.db') as db:
        db.execute('CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY NOT NULL,value TEXT NOT NULL)')
        if set_it:
            db.execute('INSERT OR REPLACE INTO settings(key,value) VALUES(?,?)', (KEY, value))
            return None
        r = db.execute('SELECT value FROM settings WHERE key=?', (KEY,)).fetchone()
    return r[0] if r else None


def start(desk, tmp, card, name, keys=()):
    """One start of the launcher on `card`, `keys` tapped after it settles,
    then quit; its log"""
    sock = Path(tempfile.gettempdir()) / f'zyperos-mp4resume-{os.getpid()}-{name}.sock'
    flags = tmp / f'{name}-flags'
    flags.mkdir(exist_ok=True)
    env = desk.card_env(card, None)
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(sock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(tmp / f'{name}-ctl'))
    env.pop('ZYPEROS_HEADLESS', None)
    logp = tmp / f'{name}.log'
    with open(logp, 'w') as log:
        p = subprocess.Popen([str(BINARY), '--device', 'pixel2'], env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            for _ in range(200):
                if sock.exists() or p.poll() is not None:
                    break
                time.sleep(0.05)
            if sock.exists():
                try:
                    ask(sock, 'wait-frames 3')
                    for k in keys:
                        ask(sock, f'tap {k}')
                        ask(sock, 'wait-frames 2')
                    ask(sock, f'shot {tmp / (name + ".png")}')
                    ask(sock, 'quit')
                    p.wait(timeout=10)
                except (OSError, subprocess.TimeoutExpired) as e:
                    check(False, f'{name}: the launcher answered its socket ({e})')
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
            if sock.exists():
                sock.unlink()
    return logp.read_text(errors='replace')


def drawn(log, text):
    return any(line.startswith('fit: drawn: ') and text in line for line in log.splitlines())


def main():
    if not BINARY.is_file():
        print('check-mp4resume: build-native/zyperos missing (make native)')
        return 1
    desk = module('desk', ROOT / 'tools/desk')
    rs = module('readme_shots', ROOT / 'tools/readme-shots.py')
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-mp4resume-'))
    try:
        card = tmp / 'card'
        rs.media_card(card)
        desk.card_sounds(card)
        subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=desk.card_env(card, None), check=True,
                       stdout=subprocess.DEVNULL)

        print('a song playing when the power went:')
        row(card, f'playing\talbum\t83.4\t{SONG}', True)
        # B to its tracks, B to MP4's shelf, B out of MP4
        log = start(desk, tmp, card, 'song', keys=('b', 'b', 'b'))
        check(f'mp4: resumed {SONG} at 83.4, paused' in log, 'the start resumes it, paused')
        check(drawn(log, 'Open Road') and drawn(log, 'Paused'), 'Now Playing opens by itself on it, Paused, and B leaves as ever')
        check(row(card) == f'paused\talbum\t83.4\t{SONG}',
              f'the row says paused now, at the same second ({row(card)!r})')
        log = start(desk, tmp, card, 'song-again')
        check('mp4: resumed' not in log, 'the next start opens nothing: it was not heard')

        print('a book and a video:')
        row(card, f'playing\talbum\t600.0\t{BOOK}', True)
        log = start(desk, tmp, card, 'book')
        check(f'mp4: resumed {BOOK} at 600.0, paused' in log and drawn(log, 'Paused') and
              row(card) == f'paused\talbum\t600.0\t{BOOK}', 'a book back on its Now Playing at 10:00, paused')
        row(card, f'playing\talbum\t42.0\t{VIDEO}', True)
        log = start(desk, tmp, card, 'video')
        check(f'mp4: resumed {VIDEO} at 42.0, paused' in log and drawn(log, 'Sintel') and
              row(card) == f'paused\talbum\t42.0\t{VIDEO}', 'a video back on its player at 0:42, paused')
        print('Shuffle All\'s song:')
        row(card, f'playing\tall\t20.0\t{SONG}', True)
        log = start(desk, tmp, card, 'all')
        check(f'mp4: resumed {SONG} at 20.0, paused' in log and row(card) == f'paused\tall\t20.0\t{SONG}',
              'Shuffle All again from that song, paused')

        print('nothing to resume:')
        row(card, f'paused\talbum\t83.4\t{SONG}', True)
        log = start(desk, tmp, card, 'paused')
        check('mp4: resumed' not in log and row(card) is not None, 'paused when the power went: nothing opens')
        row(card, 'playing\talbum\t12.0\tMusic/Gone/01 Gone.mp3', True)
        log = start(desk, tmp, card, 'gone')
        check('mp4: resumed' not in log and 'is not on the card' in log and row(card) is None,
              'its file gone: nothing opens, and the row goes')
        row(card, 'not a row', True)
        log = start(desk, tmp, card, 'broken')
        check('mp4: resumed' not in log, 'a broken row: nothing')
    finally:
        if not fails:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print(f'check-mp4resume: {len(fails)} failed (logs and shots in {tmp})')
        return 1
    print('ok: MP4 comes back paused where it was after a power-off while playing, and only then')
    return 0


if __name__ == '__main__':
    sys.exit(main())
