#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-mp4quit: MP4's Quit ends MP4 (DESIGN.md 11.14), on the real
launcher in desk mode's dummy window on the README's media card, against a
stand-in MP4 daemon on its own socket (ZYPEROS_MP4_SOCKET, the one
check-pico8-mp4 runs, which also answers STOP here).

Each walk starts from the home, its header's right of the clock shot first
(the music mark's place, empty with nothing played), opens MP4 with SELECT,
A on the first album, A on its first song (PLAY), and then:
  - paused, MENU > X (Quit's key, from any row): STOP sent, the home back
    with nothing in its header but the clock - no pause mark, no track -
    the power-off row (`mp4.resume`) gone, and SELECT opens MP4 on its shelf
    again, as at its first open, not on a Now Playing left loaded;
  - playing, MENU down to Quit and A: the same;
  - playing, SELECT let go (closes MP4, DESIGN.md 11.14): no STOP, the song
    heard behind the home with its mark in the header, as ever - which is
    also what shows the header's reading sees a mark at all.
Pixel 2 Tarik found it on 1.1.1-test.3: play, pause, MENU > Quit, and the top
bar still showed MP4's paused song on the home, MP4 closed.

Every wait is for an event, never a time: the home's or the shelf's foot
drawn (a shot's last rows that screen's own, its keys), a string drawn
(src/ui.c's "fit: drawn: ", found wherever its line begins), the daemon sent
what the key asks, a menu's row under the cursor (desk's `state`). Each has
a ceiling (CEILING) reached only when it never comes.
build-native/zyperos must be built (make native)."""
import importlib.machinery
import importlib.util
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build-native/zyperos'
KEY = 'mp4.resume'
# The panel's rows, in a Pixel 2 shot (640x480): the header's band, and the
# foot's keys under the shelf's or a list's last row
HEADER, FOOT = (0, 72), (432, 480)
# The header right of the clock: where the music mark and its track go
MARK_X = 110
# How far a foot's pixel may be from its screen's own and still be that
# screen: the shelf's glow under its keys takes the color of the cover in
# front, which loads after the first open's shot; a key's ink is 150 and
# more from the ground
NEAR = 48
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


walk = module('deskwalk', ROOT / 'tools/deskwalk.py')
p8 = module('pico8_mp4_check', ROOT / 'tools/pico8-mp4-check.py')
png = module('zyper_check', ROOT / 'tools/zyper-check.py')
CEILING = walk.CEILING


class Daemon(p8.Daemon):
    """check-pico8-mp4's stand-in mp4d, which also answers STOP as the
    daemon does (src/mp4d/mp4d.c's C_STOP: the file closed, "stopped")"""

    def command(self, line):
        if line.split('\t')[0] == 'STOP':
            with self.lock:
                self.got.append(line)
                self.state = 'stopped'
                self.send(f'STATE\tstate=stopped\tpath={self.file}')
            return
        super().command(line)


def rows(path, r0, r1, x0=0):
    """Rows r0..r1 of a shot, from x0 to the right edge, as bytes"""
    bpp, img = png.load(str(path), r1)
    return b''.join(bytes(img[y][x0 * bpp:]) for y in range(r0, r1))


def resume_row(card):
    """The power-off row as it stands while the launcher runs, read only"""
    try:
        with sqlite3.connect(f'file:{card / "userdata/zyperos.db"}?mode=ro', uri=True, timeout=5) as db:
            r = db.execute('SELECT value FROM settings WHERE key=?', (KEY,)).fetchone()
    except sqlite3.Error:
        return None
    return r[0] if r else None


def run(desk, tmp, card, name, leave):
    """One walk, on a launcher of its own; `leave` is how MP4 is left:
    'paused-x', 'playing-row' or 'select'"""
    short = Path(tempfile.gettempdir())
    msock = short / f'zyq-m-{os.getpid()}-{name}.sock'
    dsock = short / f'zyq-d-{os.getpid()}-{name}.sock'
    cdir = short / f'zyq-c-{os.getpid()}-{name}'
    cdir.mkdir(mode=0o700, exist_ok=True)
    flags = tmp / f'{name}-flags'
    flags.mkdir(exist_ok=True)
    daemon = Daemon(str(msock), str(card / 'Music/none.mp3'))
    daemon.state = 'stopped'
    env = desk.card_env(card, None)
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(dsock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(cdir),
               ZYPEROS_MP4_SOCKET=str(msock), ZYPEROS_EQ_FILE=str(tmp / f'{name}-eq'))
    env.pop('ZYPEROS_HEADLESS', None)
    logp = tmp / f'{name}.log'
    shots = {}

    def log():
        return logp.read_text(errors='replace')

    def drawn(text):
        return text in walk.drawn_lines(log())

    def shot(tag):
        path = tmp / f'{name}-{tag}.png'
        walk.ask(dsock, f'shot {path}')
        shots[tag] = path
        return path

    def foot_of(tag):
        """the screen `tag` was shot on is drawn again: its foot's keys the
        same, every pixel within NEAR of that shot's"""
        want = rows(shots[tag], *FOOT)
        return lambda: all(abs(a - b) <= NEAR for a, b in zip(rows(shot('now'), *FOOT), want))

    def waited(ok, what):
        if walk.until(ok):
            return True
        check(False, f'{name}: {what}, within {CEILING} s')
        keep = [l for l in log().splitlines() if 'fit: drawn: ' in l or l.startswith('mp4')]
        print('    ' + '\n    '.join(keep[-40:]))
        return False

    def key(k):
        walk.ask(dsock, f'press {k}')
        walk.ask(dsock, f'release {k}')

    def sent(verb):
        return lambda: any(l.split('\t')[0] == verb for l in daemon.got)

    def focus():
        s = walk.ask(dsock, 'state')
        return s.get('screen'), s.get('focus')

    with open(logp, 'w') as out:
        p = subprocess.Popen([str(BINARY), '--device', 'pixel2'], env=env, stdout=out, stderr=subprocess.STDOUT)
        try:
            ok = (waited(lambda: dsock.exists() or p.poll() is not None, 'the launcher up') and
                  dsock.exists() and waited(lambda: drawn('Open games'), 'the home drawn'))
            if ok:
                shot('home')
                key('select')
                ok = waited(lambda: drawn('MP4 / All'), 'SELECT: MP4\'s shelf drawn')
            if ok:
                shot('shelf')
                key('a')
                ok = waited(lambda: drawn('Open Road'), 'A: the album\'s tracks drawn')
            if ok:
                key('a')
                ok = (waited(sent('PLAY'), 'A on a song: PLAY sent') and
                      waited(lambda: drawn('Pause'), 'Now Playing drawn, playing'))
            if ok and leave == 'paused-x':
                key('a')
                ok = (waited(sent('PAUSE'), 'A: PAUSE sent') and
                      waited(lambda: drawn('Paused'), 'Now Playing drawn, paused'))
            if ok and leave != 'select':
                key('menu')
                ok = waited(lambda: focus()[0] == 'MP4', 'MENU: MP4\'s menu up')
            if ok and leave == 'paused-x':
                key('x')
            elif ok and leave == 'playing-row':
                for _ in range(16):
                    if focus()[1] == 'Quit':
                        break
                    was = focus()[1]
                    key('down')
                    if not waited(lambda: focus()[1] != was, 'DOWN: the next row'):
                        break
                ok = focus()[1] == 'Quit'
                check(ok, f'{name}: Quit is a row of MP4\'s menu')
                if ok:
                    key('a')
            elif ok:
                key('select')
            if ok:
                ok = waited(foot_of('home'), 'MP4 closed: the home drawn again')
            if ok:
                header = rows(shots['now'], *HEADER, MARK_X)
                clean = header == rows(shots['home'], *HEADER, MARK_X)
                stopped = sent('STOP')()
                row = resume_row(card)
                if leave == 'select':
                    check(not stopped and not clean,
                          'SELECT: MP4 closed, the song heard behind the home with its mark in the header, '
                          'no STOP (DESIGN.md 11.14)')
                    check(row is not None and row.startswith('playing\t'),
                          f'and the power-off row says playing ({row!r})')
                else:
                    how = 'paused, MENU > X' if leave == 'paused-x' else 'playing, MENU > Quit with A'
                    check(stopped, f'{how}: STOP sent, the daemon lets the song go')
                    check(clean, f'{how}: the home\'s header has no MP4 mark and no track, only the clock '
                          f'(shot: {shots["now"]})')
                    check(row is None, f'{how}: the power-off row is gone, nothing to open at a start ({row!r})')
                    key('select')
                    check(waited(foot_of('shelf'), 'SELECT again: MP4\'s shelf, as at its first open'),
                          f'{how}: SELECT opens MP4 afresh on its shelf, not on a Now Playing left loaded')
            walk.ask(dsock, 'quit')
            p.wait(timeout=CEILING)
        except (OSError, subprocess.TimeoutExpired, ValueError) as e:
            check(False, f'{name}: the launcher answered its socket ({e})')
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
            daemon.close()
            for s in (dsock,):
                if s.exists():
                    s.unlink()
            shutil.rmtree(cdir, ignore_errors=True)


def main():
    if not BINARY.is_file():
        print('check-mp4quit: build-native/zyperos missing (make native)')
        return 1
    desk = module('desk', ROOT / 'tools/desk')
    rs = module('readme_shots', ROOT / 'tools/readme-shots.py')
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-mp4quit-'))
    try:
        for name, leave, what in (('paused-x', 'paused-x', 'a song paused, MENU > X:'),
                                  ('playing-row', 'playing-row', 'a song playing, MENU > Quit:'),
                                  ('select', 'select', 'a song playing, SELECT:')):
            print(what)
            card = tmp / f'{name}-card'
            rs.media_card(card)
            desk.card_sounds(card)
            subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=desk.card_env(card, None), check=True,
                           stdout=subprocess.DEVNULL)
            run(desk, tmp, card, name, leave)
    finally:
        if not fails:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print(f'check-mp4quit: {len(fails)} failed (logs and shots in {tmp})')
        return 1
    print('ok: MP4\'s Quit ends MP4, paused or playing, and its mark leaves the header; SELECT leaves it heard')
    return 0


if __name__ == '__main__':
    sys.exit(main())
