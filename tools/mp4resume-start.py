#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-mp4resume's start: MP4 back after a power-off (DESIGN.md 11.67) on
the real launcher, in desk mode's dummy window as check-desk runs it, so no
window opens and no sound plays (the host has no daemon: what Now Playing
shows is the launcher's own, before a daemon would answer). On the README's
media card, the row the launcher keeps (mp4.resume) set as a power-off left
it, then a start:
  - playing, a song: Now Playing opens by itself on that song at its second,
    paused, and the row says paused now; B goes back to its tracks and
    SELECT closes MP4 to the home; the next start opens nothing (it was not
    heard);
  - playing, a book and a video: the same, each on its own Now Playing;
  - paused: nothing opens; a file no longer on the card: nothing, and the
    row is gone.
And a power-off in a game, the video or the song playing over it: the
start goes straight back into the game, which comes first (a Diatom of the
check's own answers it, src/resident.c), MP4 opens nothing of its own over
it, and SELECT in the menu the game comes back with is Now Playing on what
played, paused at its second, the row saying paused.
Every wait is for an event - the screen a start lands on drawn, the screen
each key opens drawn, the row saying what it should - never a time or a
count of frames: on a runner busy with other checks the walk is slower, and
says the same. And a string drawn is found wherever its line begins
(drawn_lines): on #406's Linux runner, ALSA's complaints about a missing
sound card came out in front of "fit: drawn: Paused" on its line, as SELECT
played its sound, and Now Playing drawn read as not drawn.
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
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build-native/zyperos'
KEY = 'mp4.resume'
SONG = 'Music/Demo Library/First Light/02 Open Road.mp3'
BOOK = 'Audiobooks/Treasure Island/01 The Old Sea-dog at the Admiral Benbow.mp3'
VIDEO = 'Videos/Sintel.mp4'
# The game a power-off left running, its auto state beside it: the next
# start goes back into it (src/place.c's playing_restore)
GAME_TAG, GAME_FOLDER, GAME = 'GBC', 'Game Boy Color', 'Resume Test.gbc'
fails = []
# The most any wait in a walk waits for its event (until)
CEILING = 30


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


class Diatom:
    """The resident emulator's side of its socket (src/resident.c), as much
    of it as a game resumed at the start needs and no more: READY to a
    connection, RUNNING to RUN, PAUSED to the PAUSE that comes back with the
    menu up, RUNNING to RESUME, EXIT to STOP. No core and no picture: what is
    looked at is the launcher's MP4 over the game. Every line it was sent is
    kept, in `lines`."""

    def __init__(self, path):
        self.lines = []
        self.srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.srv.bind(str(path))
        self.srv.listen(4)
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        while True:
            try:
                c, _ = self.srv.accept()
            except OSError:
                return
            threading.Thread(target=self.talk, args=(c,), daemon=True).start()

    def talk(self, c):
        answers = {'RUN': b'RUNNING\n', 'PAUSE': b'PAUSED\n', 'RESUME': b'RUNNING\n',
                   'STOP': b'EXIT\treason=user\n'}
        buf = b''
        try:
            c.sendall(b'READY\n')
            while True:
                got = c.recv(4096)
                if not got:
                    return
                buf += got
                while b'\n' in buf:
                    line, buf = buf.split(b'\n', 1)
                    text = line.decode(errors='replace')
                    self.lines.append(text)
                    if text.split('\t')[0] in answers:
                        c.sendall(answers[text.split('\t')[0]])
        except OSError:
            return
        finally:
            c.close()

    def close(self):
        self.srv.close()


def game_on(card):
    """The game, on the card from its first start: the library a start reads
    is the one the last start saved"""
    rom = card / 'Roms' / GAME_FOLDER / GAME
    rom.parent.mkdir(parents=True, exist_ok=True)
    rom.write_bytes(bytes(32768))


def game_left_running(card):
    """What POWER in a game leaves (power_off): the game's marker, asked-for
    off, and its auto state, so the next start goes back into it"""
    state = card / 'shared/.zyperos' / GAME_FOLDER / f'{GAME}.auto.state'
    state.parent.mkdir(parents=True, exist_ok=True)
    state.write_bytes(b'state')
    (card / 'ZyperOS/.playing').write_text(f'{GAME_TAG}\n{GAME}\noff\n')


def until(ok, ceiling=None):
    """A wait for an event, never for a time: until ok() says so, looked at
    every 20 ms. False only at the ceiling (CEILING), which the event is far
    inside on the busiest runner, and which is reached only when it never
    comes."""
    end = time.monotonic() + (ceiling or CEILING)
    while not ok():
        if time.monotonic() > end:
            return False
        time.sleep(0.02)
    return True


def seen(log, text):
    """`text` drawn, the whole string (src/ui.c says each one once)"""
    return text in drawn_lines(log)


def row_now(card):
    """The row as it stands while the launcher runs: read only, so nothing
    here takes the database's write lock from under the launcher's own
    writes"""
    try:
        with sqlite3.connect(f'file:{card / "userdata/zyperos.db"}?mode=ro', uri=True, timeout=5) as db:
            r = db.execute('SELECT value FROM settings WHERE key=?', (KEY,)).fetchone()
    except sqlite3.Error:
        return None
    return r[0] if r else None


def start(desk, tmp, card, name, ready, steps=(), game=False, settle=None):
    """One start of the launcher on `card`; its log. Every wait in it is for
    the event it names (until):
      - `ready`, a string the screen it starts on draws: the start is there;
      - each of `steps`, (KEY, TEXT): KEY pressed and let go (a press and a
        let-go in one pass are a tap to every screen), then TEXT drawn by
        the screen it opens - so no press lands on a screen before it reads
        keys, and none is still held at the next (desk's `tap` holds 80 ms,
        longer than a frame);
      - `settle()`, when given, saying so;
    then its picture, and quit. A wait that runs out is a failure of its own,
    with the walk's lines, since the runner's files go with it. `game`: a
    Diatom of the check's own answers on the resident's socket, kept in
    `start.diatom`."""
    sock = Path(tempfile.gettempdir()) / f'zyperos-mp4resume-{os.getpid()}-{name}.sock'
    flags = tmp / f'{name}-flags'
    flags.mkdir(exist_ok=True)
    env = desk.card_env(card, None)
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(sock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(tmp / f'{name}-ctl'))
    env.pop('ZYPEROS_HEADLESS', None)
    dsock = Path(tempfile.gettempdir()) / f'zyperos-mp4resume-{os.getpid()}-d.sock'
    if dsock.exists():
        dsock.unlink()
    # No resident on the socket a start looks at, unless this one has one
    env.update(ZYPEROS_DIATOM_SOCKET=str(dsock), ZYPEROS_DIATOM_PIDFILE=str(tmp / f'{name}-diatom.pid'))
    start.diatom = Diatom(dsock) if game else None
    logp = tmp / f'{name}.log'

    def log():
        return logp.read_text(errors='replace')

    def waited(ok, what):
        if until(ok):
            return True
        check(False, f'{name}: {what}, within {CEILING} s')
        walk_lines(log())
        return False

    with open(logp, 'w') as out:
        p = subprocess.Popen([str(BINARY), '--device', 'pixel2'], env=env, stdout=out, stderr=subprocess.STDOUT)
        try:
            if waited(lambda: sock.exists() or p.poll() is not None, 'the launcher up') and sock.exists():
                try:
                    if waited(lambda: seen(log(), ready), f'{ready!r} drawn'):
                        for key, text in steps:
                            ask(sock, f'press {key}')
                            ask(sock, f'release {key}')
                            if not waited(lambda: seen(log(), text), f'{key}: {text!r} drawn'):
                                break
                        else:
                            if settle:
                                waited(settle, 'settled')
                    ask(sock, f'shot {tmp / (name + ".png")}')
                    ask(sock, 'quit')
                    p.wait(timeout=CEILING)
                except (OSError, subprocess.TimeoutExpired) as e:
                    check(False, f'{name}: the launcher answered its socket ({e})')
                    walk_lines(log())
        finally:
            if p.poll() is None:
                p.kill()
                p.wait()
            if sock.exists():
                sock.unlink()
            if start.diatom:
                start.diatom.close()
            if dsock.exists():
                dsock.unlink()
    return log()


def walk_lines(log):
    """A walk's own lines, printed with its failure: what it drew, in order,
    and what MP4 and the resident said"""
    keep = [l for l in log.splitlines() if 'fit: drawn: ' in l or l.startswith(('mp4: ', 'diatom', 'resident'))]
    print('    ' + '\n    '.join(keep[-60:]) if keep else '    (nothing drawn)')


# Every string the launcher says it drew, wherever its line begins: the
# reading the desk walks share (tools/deskwalk.py's drawn_lines; ALSA's lines
# on a runner with no sound card can come first on a drawn string's line)
drawn_lines = module('deskwalk', ROOT / 'tools/deskwalk.py').drawn_lines


def drawn(log, text):
    return any(text in d for d in drawn_lines(log))


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
        game_on(card)
        desk.card_sounds(card)
        subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=desk.card_env(card, None), check=True,
                       stdout=subprocess.DEVNULL)

        # What each start waits for (start): Now Playing drawn ('Paused',
        # which only it says), the home ('Open games', its A), the game's
        # menu ('Continue', its first row), MP4's shelf ('MP4 / All')
        NOW, HOME, MENU, SHELF = 'Paused', 'Open games', 'Continue', 'MP4 / All'

        def says(value):
            return lambda: row_now(card) == value

        print('a song playing when the power went:')
        row(card, f'playing\talbum\t83.4\t{SONG}', True)
        # B to its tracks (a row only they draw), SELECT out of MP4 to the home
        log = start(desk, tmp, card, 'song', NOW, steps=(('b', 'Home Again'), ('select', HOME)),
                    settle=says(f'paused\talbum\t83.4\t{SONG}'))
        check(f'mp4: resumed {SONG} at 83.4, paused' in log, 'the start resumes it, paused')
        check(drawn(log, 'Open Road') and seen(log, NOW) and seen(log, HOME),
              'Now Playing opens by itself on it, Paused; B goes back to its tracks and SELECT leaves, as ever')
        check(row(card) == f'paused\talbum\t83.4\t{SONG}',
              f'the row says paused now, at the same second ({row(card)!r})')
        log = start(desk, tmp, card, 'song-again', HOME)
        check('mp4: resumed' not in log, 'the next start opens nothing: it was not heard')

        print('a book and a video:')
        row(card, f'playing\talbum\t600.0\t{BOOK}', True)
        log = start(desk, tmp, card, 'book', NOW, settle=says(f'paused\talbum\t600.0\t{BOOK}'))
        check(f'mp4: resumed {BOOK} at 600.0, paused' in log and seen(log, NOW) and
              row(card) == f'paused\talbum\t600.0\t{BOOK}', 'a book back on its Now Playing at 10:00, paused')
        row(card, f'playing\talbum\t42.0\t{VIDEO}', True)
        log = start(desk, tmp, card, 'video', NOW, settle=says(f'paused\talbum\t42.0\t{VIDEO}'))
        check(f'mp4: resumed {VIDEO} at 42.0, paused' in log and drawn(log, 'Sintel') and
              row(card) == f'paused\talbum\t42.0\t{VIDEO}', 'a video back on its player at 0:42, paused')
        print('Shuffle All\'s song:')
        row(card, f'playing\tall\t20.0\t{SONG}', True)
        log = start(desk, tmp, card, 'all', NOW, settle=says(f'paused\tall\t20.0\t{SONG}'))
        check(f'mp4: resumed {SONG} at 20.0, paused' in log and row(card) == f'paused\tall\t20.0\t{SONG}',
              'Shuffle All again from that song, paused')

        # The home drawn is the start over: a resume comes before it
        print('nothing to resume:')
        row(card, f'paused\talbum\t83.4\t{SONG}', True)
        log = start(desk, tmp, card, 'paused', HOME)
        check('mp4: resumed' not in log and row(card) is not None, 'paused when the power went: nothing opens')
        row(card, 'playing\talbum\t12.0\tMusic/Gone/01 Gone.mp3', True)
        log = start(desk, tmp, card, 'gone', HOME)
        check('mp4: resumed' not in log and 'is not on the card' in log and row(card) is None,
              'its file gone: nothing opens, and the row goes')
        row(card, 'not a row', True)
        log = start(desk, tmp, card, 'broken', HOME)
        check('mp4: resumed' not in log, 'a broken row: nothing')

        # Mine, 1.1.0-rc.3: "I powered off while having a video playing in
        # mp4 and running a gbc game and when i clicked select in the menu
        # after powering on again it didnt show the video playing that video
        # paused"
        print('a video playing over a game when the power went:')
        game_left_running(card)
        row(card, f'playing\talbum\t42.0\t{VIDEO}', True)
        log = start(desk, tmp, card, 'game-video', MENU, game=True, settle=says(f'paused\talbum\t42.0\t{VIDEO}'))
        ran = [l for l in start.diatom.lines if l.startswith('RUN\t')]
        check(len(ran) == 1 and f'{GAME_FOLDER}/{GAME}' in ran[0] and f'{GAME}.auto.state' in ran[0] and
              'PAUSE' in start.diatom.lines, 'the start goes straight back into the game, at its state, the menu up')
        check(f'mp4: resumed {VIDEO} at 42.0, paused' in log,
              'MP4 holds the video under it, paused at its second')
        # Before the game's menu, which is up: Now Playing's Paused never
        # drawn. (Its name is drawn once the game is over, on the shelf it
        # comes back to, as a song paused is.)
        check(not drawn(log, NOW), 'and opens nothing of its own over the game, which comes first')
        check(row(card) == f'paused\talbum\t42.0\t{VIDEO}',
              f'the row says paused now, at the same second ({row(card)!r})')
        game_left_running(card)
        row(card, f'playing\talbum\t42.0\t{VIDEO}', True)
        log = start(desk, tmp, card, 'game-video-select', MENU, steps=(('select', NOW),), game=True)
        check(f'mp4: resumed {VIDEO} at 42.0, paused' in log and drawn(log, 'Sintel') and seen(log, NOW),
              "SELECT in the game's menu: Now Playing on the video, Paused")
        game_left_running(card)
        # SELECT with nothing held: MP4's shelf
        log = start(desk, tmp, card, 'game-video-again', MENU, steps=(('select', SHELF),), game=True)
        check('mp4: resumed' not in log and not drawn(log, NOW),
              'the next start opens nothing: it was not heard')

        print('a song playing over a game:')
        game_left_running(card)
        row(card, f'playing\talbum\t83.4\t{SONG}', True)
        log = start(desk, tmp, card, 'game-song', MENU, steps=(('select', NOW),), game=True,
                    settle=says(f'paused\talbum\t83.4\t{SONG}'))
        check(f'mp4: resumed {SONG} at 83.4, paused' in log, 'MP4 holds the song under it, paused at its second')
        check(drawn(log, 'Open Road') and seen(log, NOW), "SELECT in the game's menu: Now Playing on the song, Paused")
        check(row(card) == f'paused\talbum\t83.4\t{SONG}',
              f'the row says paused now, at the same second ({row(card)!r})')
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
