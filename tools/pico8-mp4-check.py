#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-pico8-mp4: MP4 over your PICO-8 (DESIGN.md 11.55, src/mp4hand.h), as
over a Diatom game: it keeps playing, and MENU then SELECT opens it.

1. launch.sh's run_port, on a copy with the card and /tmp in a scratch folder
   and stand-ins for the launcher, the program, portkeys and killall (as
   check-launch-start does): for a request whose fifth field is "mp4" (your
   PICO-8's) MP4's daemon is not stopped, the follower (zyperos.elf
   --mp4-follow) starts beside the program, portkeys gets --follow with its
   pid, and the follower is ended before the launcher comes back; for any
   other program (OpenBOR's request, four fields) MP4 is stopped as before
   and there is no follower.
2. The real launcher (build-native/zyperos, make native) in desk mode's
   dummy window on the README's media card, against a stand-in MP4 daemon
   on its own socket (ZYPEROS_MP4_SOCKET), a card with your PICO-8 on it:
   - a start with a queue handed over takes it on where the daemon is, sends
     no PLAY, opens nothing by itself (no resume), and removes the file; an
     END then plays the next track, as the launcher's own queue would;
   - a PICO-8 cart opened (px2-open's channel) writes the request with
     "mp4" fifth and the queue to the file, at the track playing;
   - the follower takes the queue, sends the next track at an END, and
     writes the file as it ends, its lines in the log as "port: " ones,
     which plastron's splash leaves off the program's loading screen;
   - the in-game menu over PICO-8, with the follower stopped as portkeys
     stops it: SELECT opens MP4 on Now Playing of the song playing, L1 is
     the song before, SELECT let go closes MP4 back to the menu, B is
     Continue, and the file is written back; the follower, let go on, takes
     that queue and plays on from it;
   - and over a program MP4 is stopped for (no file), SELECT opens nothing.
Every wait is for an event, never a time: the program stand-in runs until
portkeys and the follower beside it have said they are up; a start until
its screen is drawn; a key until the launcher has taken it and drawn the
frame after; Now Playing until the daemon's song is drawn; the daemon and
the follower until they have said or done the thing waited for. Each with a
ceiling (CEILING) reached only when it never comes. A string drawn is found
wherever its line begins (drawn_lines: ALSA's lines on Linux).
"""
import importlib.machinery
import importlib.util
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build-native/zyperos'
ALBUM = 'Music/Demo Library/First Light/'
TRACKS = [ALBUM + t for t in ('01 First Light.mp3', '02 Open Road.mp3', '03 Home Again.mp3')]
fails = []
# The most any wait waits for its event (wait_for): far past what a step
# takes on the busiest runner, and reached only when the event never comes
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


# ---- 1. launch.sh ---------------------------------------------------------

def launch_sh(tmp):
    """run_port for your PICO-8 and for OpenBOR, with stand-ins"""
    d = tmp / 'sh'
    card, bin_ = d / 'card', d / 'bin'
    calls = d / 'calls'
    zy = card / 'ZyperOS'

    def edit(src):
        text = src.read_text()
        for pat in ('/mnt/SDCARD', '/tmp/', 'exec setsid /bin/bash', 'killall -TERM mp4d'):
            if src.name == 'launch.sh' and pat not in text:
                check(False, f'launch.sh no longer has {pat!r}')
        # /tmp/ first: the scratch folder is itself under /tmp on Linux, and
        # the card's path put in first would be moved again
        return (text.replace('/tmp/', f'{d}/tmp/').replace('/mnt/SDCARD', str(card))
                .replace('/usr/share/zyperos/card', str(d / 'seed')).replace('/dev/kmsg', str(d / 'kmsg')))

    def put(path, text, run=True):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        if run:
            path.chmod(0o755)

    def run(request):
        shutil.rmtree(d, ignore_errors=True)
        for p in (zy, bin_, d / 'tmp', card / '.userdata/pixel2', card / 'Roms/PICO-8'):
            p.mkdir(parents=True, exist_ok=True)
        put(zy / 'launch.sh', edit(ROOT / 'sd/zyperos/launch.sh'), False)
        put(zy / 'device.sh', edit(ROOT / 'sd/pixel2/device.sh') + '\ndevice_radios() { :; }\n', False)
        for c in ('poweroff', 'reboot'):
            put(bin_ / c, f'#!/bin/sh\necho {c} >> "{calls}"\n')
        put(bin_ / 'pidof', '#!/bin/sh\nexit 1\n')
        put(bin_ / 'dmesg', '#!/bin/sh\n:\n')
        put(bin_ / 'killall', f'#!/bin/sh\necho "killall $*" >> "{calls}"\n')
        # No setsid on a Mac: the program runs in launch.sh's group here
        put(bin_ / 'setsid', '#!/bin/sh\nexec "$@"\n')
        put(zy / 'diatom', '#!/bin/sh\nexit 0\n')
        # The launcher: the request on its first run, a power-off on its
        # second; and the follower, which says it started and when it ends
        put(zy / 'zyperos.elf', f'''#!/bin/sh
case $1 in
--mp4-follow)
	trap 'echo "follow TERM" >> "{calls}"; exit 0' TERM
	echo "follow start $$" >> "{calls}"
	while :; do sleep 0.05; done ;;
--port-menu) exit 10 ;;
esac
n=$(cat "{d}/runs" 2> /dev/null || echo 0); n=$((n + 1)); echo $n > "{d}/runs"
echo "launcher $n" >> "{calls}"
if [ $n = 1 ]; then printf '%s\\n' '{request}' > "{d}/tmp/zyperos_port"; else : > "{d}/tmp/zyperos_poweroff"; fi
exit 0
''')
        put(zy / 'portkeys', f'''#!/bin/sh
trap 'exit 0' TERM
echo "portkeys $*" >> "{calls}"
while :; do sleep 0.05; done
''')
        # The program runs until what launch.sh starts beside it has said it
        # is up (portkeys, and your PICO-8's follower), its TERM trap set
        # first, at most CEILING: an event, not a time, so a busy runner
        # cannot end the program before they start
        for script, beside in (('pico8/PICO-8.sh', '^portkeys |^follow start '), ('openbor/OpenBOR.sh', '^portkeys ')):
            need = len(beside.split('|'))
            put(zy / script, f'''#!/bin/bash
echo "program {script} $*" >> "{calls}"
for i in $(seq {CEILING * 20}); do
	[ "$(grep -cE '{beside}' "{calls}")" -ge {need} ] && break
	sleep 0.05
done
exit 0
''')
        env = dict(os.environ, PATH=f'{bin_}:{os.environ["PATH"]}')
        try:
            subprocess.run(['sh', str(zy / 'launch.sh')], env=env, stdin=subprocess.DEVNULL,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2 * CEILING)
        except subprocess.TimeoutExpired:
            check(False, f'launch.sh ended (it ran {2 * CEILING} s)')
        return calls.read_text().splitlines() if calls.exists() else []

    print('launch.sh, your PICO-8 (a request with "mp4" fifth):')
    tab = '\t'
    lines = run(tab.join((f'{zy}/pico8/PICO-8.sh', f'{card}/Roms/PICO-8/celeste.p8', 'PICO8', 'stretch', 'mp4')))
    check(any(l.startswith('program pico8/PICO-8.sh ') for l in lines), 'the program ran')
    check('killall -TERM mp4d' not in lines, 'MP4 is not stopped for it')
    start = next((l for l in lines if l.startswith('follow start ')), None)
    check(start is not None, 'the follower started beside it')
    pid = start.split()[-1] if start else '?'
    keys = next((l for l in lines if l.startswith('portkeys ')), '')
    check(keys.startswith(f'portkeys --follow {pid} --menu {zy}/zyperos.elf PICO8 '),
          f'portkeys has --follow with the follower\'s pid, then --menu ({keys!r})')
    check('follow TERM' in lines and 'launcher 2' in lines and
          lines.index('follow TERM') < lines.index('launcher 2'),
          'the follower is ended, and waited for, before the launcher comes back')

    print('launch.sh, OpenBOR (four fields):')
    lines = run(tab.join((f'{zy}/openbor/OpenBOR.sh', f'{card}/Roms/OPENBOR/x.pak', 'OPENBOR', 'stretch')))
    check(any(l.startswith('program openbor/OpenBOR.sh ') for l in lines), 'the program ran')
    check('killall -TERM mp4d' in lines, 'MP4 is stopped for it, as before')
    check(not any(l.startswith('follow start') for l in lines), 'no follower')
    keys = next((l for l in lines if l.startswith('portkeys ')), '')
    check(keys.startswith(f'portkeys --menu {zy}/zyperos.elf OPENBOR '), f'portkeys as before ({keys!r})')


# ---- 2. the launcher ------------------------------------------------------

class Daemon:
    """A stand-in mp4d (src/mp4d/mp4d.c's protocol): one connection at a
    time, the newest kept; what it is sent, in order"""

    def __init__(self, path, playing):
        self.path, self.state, self.file, self.at, self.len = path, 'playing', playing, 1.0, 200.0
        self.got, self.cfd, self.lock, self.stop = [], None, threading.Lock(), False
        if os.path.exists(path):
            os.unlink(path)
        self.lsock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.lsock.bind(path)
        self.lsock.listen(2)
        threading.Thread(target=self.accept, daemon=True).start()
        threading.Thread(target=self.tick, daemon=True).start()

    def send(self, line):
        if self.cfd:
            try:
                self.cfd.sendall((line + '\n').encode())
            except OSError:
                pass

    def accept(self):
        while not self.stop:
            try:
                c, _ = self.lsock.accept()
            except OSError:
                return
            with self.lock:
                if self.cfd:
                    self.cfd.close()
                self.cfd = c
                self.got.append('<connect>')
                self.send('READY\tproto=7')
                if self.state != 'stopped':
                    self.send(f'META\ttitle={Path(self.file).stem[3:]}\tartist=\talbum=\tlen={self.len:.1f}'
                              '\tchapters=0\tvideo=0x0')
                self.send(f'STATE\tstate={self.state}\tpath={self.file}')
                self.send(f'POS\tat={self.at:.1f}\tlen={self.len:.1f}')
                self.send('SINK\tdevice=default')
            threading.Thread(target=self.read, args=(c,), daemon=True).start()

    def read(self, c):
        buf = b''
        while True:
            try:
                b = c.recv(4096)
            except OSError:
                b = b''
            if not b:
                return
            buf += b
            while b'\n' in buf:
                line, buf = buf.split(b'\n', 1)
                self.command(line.decode())

    def command(self, line):
        with self.lock:
            if line.startswith('COVER'):
                f = dict(kv.split('=', 1) for kv in line.split('\t')[1:] if '=' in kv)
                self.send(f'COVER\tbase={f.get("base", "")}\tfile=')
                return
            self.got.append(line)
            f = dict(kv.split('=', 1) for kv in line.split('\t')[1:] if '=' in kv)
            verb = line.split('\t')[0]
            if verb == 'PLAY':
                self.file, self.at, self.state = f['path'], float(f.get('at', 0)), 'playing'
                self.send(f'META\ttitle={Path(self.file).stem[3:]}\tartist=\talbum=\tlen={self.len:.1f}'
                          '\tchapters=0\tvideo=0x0')
                self.send(f'STATE\tstate=playing\tpath={self.file}')
            elif verb == 'SEEK':
                self.at = float(f.get('at', 0))
                self.send(f'POS\tat={self.at:.1f}\tlen={self.len:.1f}')
            elif verb in ('SINK', 'CHANNELS'):
                k = 'device' if verb == 'SINK' else 'mode'
                self.send(f'{verb}\t{k}={f.get(k, "")}')

    def end(self):
        """the file plays out by itself"""
        with self.lock:
            self.got.append('<end>')
            self.state = 'stopped'
            self.send(f'END\tpath={self.file}')
            self.send(f'STATE\tstate=stopped\tpath={self.file}')

    def plays(self):
        return [l.split('\tpath=')[1].split('\t')[0] for l in self.got if l.startswith('PLAY\t')]

    def tick(self):
        """POS while playing, the second held where PLAY put it, so what L1
        does (the song before, under 3 s) is the same however slow the
        computer draws"""
        while not self.stop:
            time.sleep(0.25)
            with self.lock:
                if self.state == 'playing':
                    self.send(f'POS\tat={self.at:.1f}\tlen={self.len:.1f}')

    def close(self):
        self.stop = True
        self.lsock.close()
        if os.path.exists(self.path):
            os.unlink(self.path)


def ask(sock, line, timeout=CEILING):
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


def wait_for(cond, seconds=CEILING):
    """A wait for an event, never for a time: until cond() says so, looked
    at every 50 ms, False only at the ceiling"""
    until = time.monotonic() + seconds
    while time.monotonic() < until:
        if cond():
            return True
        time.sleep(0.05)
    return cond()


# Every string the launcher says it drew, wherever its line begins: the
# reading the desk walks share (tools/deskwalk.py's drawn_lines; ALSA's lines
# on a runner with no sound card can come first on a drawn string's line)
drawn_lines = module('deskwalk', ROOT / 'tools/deskwalk.py').drawn_lines


def hand_text(card, pos):
    """The file as the launcher writes it: the album, in order, at `pos`"""
    return ('ZMQ1\nstate\tplaying\nat\t12.0\nmode\t0\nseed\t1\nbook\t0\nvideos\t0\nnames\t0\nall\t0\n'
            f'speed\t1.00\nroot\t{card}\nartist\tDemo Library\nalbum\tFirst Light\nn\t3\npos\t{pos}\n'
            'order\t0 1 2\n' + ''.join(f'track\t{t}\n' for t in TRACKS))


def field(text, key):
    for line in text.splitlines():
        if line.startswith(key + '\t'):
            return line.split('\t', 1)[1]
    return None


def launcher(tmp):
    desk = module('desk', ROOT / 'tools/desk')
    rs = module('readme_shots', ROOT / 'tools/readme-shots.py')
    card, flags = tmp / 'card', tmp / 'flags'
    flags.mkdir(parents=True)
    rs.media_card(card)
    desk.card_sounds(card)
    subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=desk.card_env(card, None), check=True,
                   stdout=subprocess.DEVNULL)
    # Your PICO-8 on the card: its two files and its script, and a cart
    (card / 'Roms/PICO-8').mkdir(parents=True, exist_ok=True)
    (card / 'Roms/PICO-8/celeste.p8').write_text('pico-8 cartridge\n')
    (card / 'Bios/PICO-8').mkdir(parents=True, exist_ok=True)
    for f in ('pico8_64', 'pico8.dat'):
        (card / 'Bios/PICO-8' / f).write_bytes(b'stand-in')
    (card / 'ZyperOS/pico8').mkdir(parents=True, exist_ok=True)
    (card / 'ZyperOS/pico8/PICO-8.sh').write_text('#!/bin/sh\n')
    hand, request = flags / 'zyperos_mp4_queue', flags / 'zyperos_port'
    short = Path(tempfile.gettempdir())
    msock = short / f'zyp8m-{os.getpid()}.sock'
    dsock = short / f'zyp8d-{os.getpid()}.sock'
    cdir = short / f'zyp8c-{os.getpid()}'
    cdir.mkdir(mode=0o700, exist_ok=True)
    daemon = Daemon(str(msock), str(card / TRACKS[0]))
    env = desk.card_env(card, None)
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(dsock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(cdir),
               ZYPEROS_MP4_SOCKET=str(msock))
    env.pop('ZYPEROS_HEADLESS', None)
    procs = []

    def start(name, args):
        if dsock.exists():
            dsock.unlink()
        log = open(tmp / f'{name}.log', 'w')
        p = subprocess.Popen([str(BINARY)] + args, env=env, stdout=log, stderr=subprocess.STDOUT)
        procs.append(p)
        return p

    def logged(name):
        return (tmp / f'{name}.log').read_text(errors='replace')

    def desk_up(p, name, ready):
        """Its control socket, then `ready` drawn: the screen it starts on is
        up and reads keys"""
        return (wait_for(lambda: dsock.exists() or p.poll() is not None) and dsock.exists() and
                wait_for(lambda: ready in drawn_lines(logged(name))))

    def key(k, last=False):
        """`k` pressed and let go, each said back once the launcher's loop
        has taken it, then the next frame drawn: the pass that read them is
        over, whatever it did, before anything more is sent. Without that a
        press sent at once could be read on the same pass as this let-go -
        Now Playing reads B before SELECT's let-go, the menu B before SELECT
        - and no press is still down at the next. Events, not a time.
        `last`: the key that ends the launcher (B, Continue), whose event is
        its end (ended), with no frame after it to wait for"""
        ask(dsock, f'press {k}')
        ask(dsock, f'release {k}')
        if not last:
            ask(dsock, 'wait-frames 1')

    def ended(p, seconds=CEILING):
        try:
            p.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            return None
        return p.returncode

    try:
        print('the launcher back from your PICO-8, a queue handed over:')
        hand.write_text(hand_text(card, 0))
        p = start('start', ['--device', 'pixel2'])
        # The home's A drawn: the start is over (a queue taken on, a resume)
        check(desk_up(p, 'start', 'Open games'), 'the launcher started')
        log = logged('start')
        check('mp4: took over 3 tracks, playing, at 1' in log, 'it takes the queue on')
        check(not hand.exists(), 'and removes the file')
        check('mp4: resumed' not in log, 'and resumes nothing: no power-off came')
        check(wait_for(lambda: '<connect>' in daemon.got) and not daemon.plays(),
              'it talks to the daemon and sends no PLAY: the song plays on')
        daemon.end()
        check(wait_for(lambda: daemon.plays() == [str(card / TRACKS[1])]),
              f'an END: the next track, from the queue taken on ({daemon.plays()})')

        print('a PICO-8 cart opened:')
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(CEILING)
        s.connect(str(cdir / 'ctl.sock'))
        s.sendall(f'open\tpath={card}/Roms/PICO-8/celeste.p8\n'.encode())
        said = s.makefile().readline().strip()
        s.close()
        check(said.startswith('ok') and 'core=run:ZyperOS/pico8/PICO-8.sh' in said,
              f'it opens on your PICO-8 ({said[:60]!r})')
        check(ended(p) == 0, 'the launcher ends for it')
        req = request.read_text().rstrip('\n').split('\t') if request.exists() else []
        check(len(req) == 5 and req[2] == 'PICO8' and req[4] == 'mp4',
              f'the request says MP4 plays on, fifth ({req[2:]})')
        h = hand.read_text() if hand.exists() else ''
        check(field(h, 'pos') == '1' and field(h, 'state') == 'playing' and field(h, 'n') == '3',
              'the queue is in the file, on the song playing')

        print('the follower beside it:')
        f = start('follow', ['--mp4-follow'])
        check(wait_for(lambda: 'mp4 follow: beside the program' in logged('follow')) and
              'mp4: took over 3 tracks, playing, at 2' in logged('follow'), 'it takes the queue')
        check(wait_for(lambda: daemon.got.count('<connect>') == 2), 'and talks to the daemon')
        daemon.end()
        check(wait_for(lambda: daemon.plays()[-1:] == [str(card / TRACKS[2])]),
              'an END: the next track')

        print('MENU over PICO-8, the follower stopped as portkeys stops it:')
        f.send_signal(signal.SIGSTOP)
        m = start('menu', ['--port-menu', 'PICO8', f'{card}/Roms/PICO-8/celeste.p8'])
        check(desk_up(m, 'menu', 'Continue'), 'the in-game menu is up')
        key('select')
        # Now Playing first says the song the file handed over, then the one
        # the daemon says it plays: that one drawn is the event
        check(wait_for(lambda: {'Home Again', '3 / 3'} <= set(drawn_lines(logged('menu')))),
              'and Now Playing on the song the daemon plays')
        log = logged('menu')
        # The file says the song the launcher handed over, the second; the
        # follower went on to the third, which the daemon says it plays
        check('port menu: MP4 ready' in log and 'mp4: took over 3 tracks, playing, at 2' in log,
              'SELECT: MP4 made ready, the queue taken from the file')
        # L1 under 3 s into a song: the one before. Let go at the
        # launcher's next pass (desk's tap, 1 ms), not when this script gets
        # round to it: L1 held 300 ms repeats. The PLAY it sends comes from
        # the pass that read it, so SELECT below is read on a later one
        n = len(daemon.plays())
        ask(dsock, 'tap l1 1')
        check(wait_for(lambda: daemon.plays()[n:] == [str(card / TRACKS[1])]),
              f'L1: the song before ({[g[:40] for g in daemon.got[-6:]]})')
        # SELECT let go closes MP4 on the pass that reads the let-go; B on a
        # pass after it is Continue, not Now Playing's way back
        key('select')
        key('b', last=True)
        rc = ended(m)
        check(rc == 10, f'SELECT let go closes MP4, and B is Continue ({rc})')
        h = hand.read_text() if hand.exists() else ''
        check(field(h, 'pos') == '1', f'the menu writes the queue back, on the song before ({field(h, "pos")})')
        # The follower's connection went to the menu's (the stand-in keeps
        # the newest), so an END reaches it once it has taken the queue again
        # and connected again
        connects = daemon.got.count('<connect>')
        f.send_signal(signal.SIGCONT)
        check(wait_for(lambda: 'the queue again, from the in-game menu' in logged('follow')),
              'the follower, let go on, takes the queue again')
        check(wait_for(lambda: daemon.got.count('<connect>') > connects), 'and talks to the daemon again')
        n = len(daemon.plays())
        daemon.end()
        check(wait_for(lambda: daemon.plays()[n:] == [str(card / TRACKS[2])]),
              'and plays on from it: an END, the next track')
        f.send_signal(signal.SIGTERM)
        check(ended(f) == 0, 'TERM ends it')
        h = hand.read_text() if hand.exists() else ''
        check(field(h, 'pos') == '2' and 'mp4: handed over 3 tracks, playing, at 3' in logged('follow'),
              'writing the queue for the launcher after')
        lines = [l for l in logged('follow').splitlines() if l]
        check(lines and all(l.startswith('port: ') for l in lines),
              'its every line a "port: " one, which splash passes over on the loading screen')

        print('MENU over a program MP4 is stopped for (no file):')
        hand.unlink()
        m = start('menu-other', ['--port-menu', 'PICO8', f'{card}/Roms/PICO-8/celeste.p8'])
        check(desk_up(m, 'menu-other', 'Continue'), 'the in-game menu is up')
        n = len(daemon.got)
        # SELECT read and done with on its pass, B on a later one (the menu
        # reads B first)
        key('select')
        key('b', last=True)
        rc = ended(m)
        log = logged('menu-other')
        check(rc == 10 and 'MP4 ready' not in log and '<connect>' not in daemon.got[n:],
              f'SELECT opens nothing, and B is Continue ({rc})')
    finally:
        for p in procs:
            if p.poll() is None:
                p.send_signal(signal.SIGCONT)
                p.kill()
                p.wait()
        daemon.close()
        shutil.rmtree(cdir, ignore_errors=True)
        if dsock.exists():
            dsock.unlink()


def main():
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-pico8-mp4-'))
    try:
        launch_sh(tmp)
        if BINARY.is_file():
            launcher(tmp)
        else:
            check(False, 'build-native/zyperos is there (make native)')
    finally:
        if not fails:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print(f'check-pico8-mp4: {len(fails)} failed (logs in {tmp})')
        return 1
    print('ok: MP4 plays on over your PICO-8, and MENU then SELECT opens it')
    return 0


if __name__ == '__main__':
    sys.exit(main())
