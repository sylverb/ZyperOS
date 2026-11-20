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
                .replace('/usr/share/zyperos/card', str(d / 'seed')).replace('/dev/kmsg', str(d / 'kmsg'))
                .replace('/usr/lib/ladspa/zyperos_eq.so', str(d / 'zyperos_eq.so'))
                .replace('/proc/asound/card1/usbid', str(d / 'card1-usbid'))
                .replace('/etc/asound.conf', str(d / 'asound.conf')))

    def put(path, text, run=True):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        if run:
            path.chmod(0o755)

    def run(request, quiet=None, dac=False, usb_pcm=True):
        shutil.rmtree(d, ignore_errors=True)
        for p in (zy, bin_, d / 'tmp', card / '.userdata/pixel2', card / 'Roms/PICO-8'):
            p.mkdir(parents=True, exist_ok=True)
        put(zy / 'launch.sh', edit(ROOT / 'sd/zyperos/launch.sh'), False)
        # The equalizer's chain on, as on a card since 1.1.0: its plugin and
        # the launcher's file; and the launcher's quiet mark when it made
        # the control
        put(zy / 'eq-port.sh', (ROOT / 'sd/zyperos/eq-port.sh').read_text(), False)
        put(d / 'zyperos_eq.so', 'plugin', False)
        (d / 'eq').write_bytes(b'ZEQ1' + bytes(12))
        if quiet is not None:
            put(d / 'tmp/zyperos_port_quiet', quiet + '\n', False)
        # The system's ALSA config, the Pixel 2's (plastron's), with or
        # without its DAC chain; and a DAC in, card 1 with a USB id
        conf = (ROOT / 'plastron/buildroot/board/px2/rootfs-overlay/etc/asound.conf').read_text()
        if not usb_pcm:
            conf = '\n'.join(l for l in conf.splitlines() if not l.startswith('pcm.usb'))
        put(d / 'asound.conf', conf, False)
        if dac:
            put(d / 'card1-usbid', '05ac:110a\n', False)
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
	echo "follow start $$" >> "{calls}"
	trap 'echo "follow TERM" >> "{calls}"; exit 0' TERM
	while :; do sleep 0.05; done ;;
--port-menu) exit 10 ;;
esac
n=$(cat "{d}/runs" 2> /dev/null || echo 0); n=$((n + 1)); echo $n > "{d}/runs"
echo "launcher $n" >> "{calls}"
if [ $n = 1 ]; then printf '%s\\n' '{request}' > "{d}/tmp/zyperos_port"; else : > "{d}/tmp/zyperos_poweroff"; fi
exit 0
''')
        put(zy / 'portkeys', f'''#!/bin/sh
echo "portkeys $*" >> "{calls}"
trap 'exit 0' TERM
while :; do sleep 0.05; done
''')
        for script in ('pico8/PICO-8.sh', 'openbor/OpenBOR.sh'):
            put(zy / script, f'#!/bin/bash\necho "program {script} $*" >> "{calls}"\nsleep 0.3\nexit 0\n')
        env = dict(os.environ, PATH=f'{bin_}:{os.environ["PATH"]}', ZYPEROS_EQ_FILE=str(d / 'eq'))
        try:
            subprocess.run(['sh', str(zy / 'launch.sh')], env=env, stdin=subprocess.DEVNULL,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=30)
        except subprocess.TimeoutExpired:
            check(False, 'launch.sh ended (it ran 30 s)')
        return calls.read_text().splitlines() if calls.exists() else []

    def chain():
        """the PCMs launch.sh gave the program"""
        conf = d / 'tmp/zyperos_port_eq.conf'
        return conf.read_text() if conf.exists() else ''

    def has_quiet(text, name):
        return (f'slave.pcm "plug:zyperos_quiet"' in text and 'pcm.zyperos_quiet {' in text and
                'type softvol' in text and f'name "{name}"' in text and 'slave.pcm "mixed"' in text)

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

    check('pcm.zyperos_eq' in chain() and 'zyperos_quiet' not in chain(),
          'no quiet mark (no control made): the equalizer\'s chain as before, PICO-8 heard')
    pico8 = tab.join((f'{zy}/pico8/PICO-8.sh', f'{card}/Roms/PICO-8/celeste.p8', 'PICO8', 'stretch', 'mp4'))
    run(pico8, quiet='Port Playback Volume')
    check(has_quiet(chain(), 'Port Playback Volume'),
          'the launcher\'s quiet mark: a softvol stage on its control between the plugin and the mix')
    log = (card / '.userdata/pixel2/logs/zyperos.log').read_text(errors='replace')
    check('quiet while MP4 plays (Port Playback Volume)' in log, 'and the log says so')
    run(pico8, quiet='Port "Playback" $(Volume)')
    check('pcm.zyperos_eq' in chain() and 'zyperos_quiet' not in chain(),
          'a mark that is no control\'s name: the chain as before')

    print('launch.sh, a USB-C DAC in:')
    run(pico8, quiet='Port Playback Volume', dac=True)
    text = chain()
    check('slave.pcm "plug:zyperos_quiet"' in text and 'slave.pcm "usb"' in text and
          'slave.pcm "mixed"' not in text, 'PICO-8 into the DAC\'s chain, "usb", through the quiet stage')
    log = (card / '.userdata/pixel2/logs/zyperos.log').read_text(errors='replace')
    check('(into usb)' in log, 'and the log says so')
    run(pico8, dac=True)
    check('slave.pcm "plug:usb"' in chain() and 'mixed' not in chain(), 'with no quiet mark, straight into it')
    run(pico8, dac=True, usb_pcm=False)
    check('slave.pcm "plug:mixed"' in chain() and '"usb"' not in chain() and 'plug:usb' not in chain(),
          'an image whose asound.conf has no DAC chain: the mix, as before')
    run(pico8)
    check('slave.pcm "plug:mixed"' in chain() and 'usb' not in chain(), 'no DAC: the mix, as before')
    run(tab.join((f'{zy}/openbor/OpenBOR.sh', f'{card}/Roms/OPENBOR/x.pak', 'OPENBOR', 'stretch')), dac=True)
    check('slave.pcm "plug:usb"' in chain(), 'and every port follows the DAC, OpenBOR too')

    print('launch.sh, OpenBOR (four fields):')
    lines = run(tab.join((f'{zy}/openbor/OpenBOR.sh', f'{card}/Roms/OPENBOR/x.pak', 'OPENBOR', 'stretch')),
                quiet='Port Playback Volume')
    check(any(l.startswith('program openbor/OpenBOR.sh ') for l in lines), 'the program ran')
    check('killall -TERM mp4d' in lines, 'MP4 is stopped for it, as before')
    check(not any(l.startswith('follow start') for l in lines), 'no follower')
    keys = next((l for l in lines if l.startswith('portkeys ')), '')
    check(keys.startswith(f'portkeys --menu {zy}/zyperos.elf OPENBOR '), f'portkeys as before ({keys!r})')
    check('pcm.zyperos_eq' in chain() and 'zyperos_quiet' not in chain(),
          'and no quiet stage, a mark there or not')


def eq_port(tmp):
    """eq-port.sh's PCMs with ZYPEROS_PORT_QUIET and without"""
    def out(quiet=None):
        env = dict(os.environ)
        env.pop('ZYPEROS_PORT_QUIET', None)
        if quiet is not None:
            env['ZYPEROS_PORT_QUIET'] = quiet
        return subprocess.run(['sh', str(ROOT / 'sd/zyperos/eq-port.sh'), 'mixed'], env=env,
                              capture_output=True, text=True).stdout

    print('eq-port.sh, the quiet stage:')
    plain, quiet = out(), out('Port Playback Volume')
    check('zyperos_quiet' not in plain and 'slave.pcm "plug:mixed"' in plain, 'none asked: as before')
    stage = quiet[quiet.find('pcm.zyperos_quiet {'):]
    check('slave.pcm "plug:zyperos_quiet"' in quiet and 'slave.pcm "plug:mixed"' not in quiet,
          'asked: the plugin plays into the stage')
    check(all(f in stage for f in ('type softvol', 'slave.pcm "mixed"', 'name "Port Playback Volume"',
                                   'card 0', 'min_dB -95.625', 'max_dB 0.0', 'resolution 256')),
          'a softvol on that control of card 0, with asound.conf\'s numbers for the USB one, into the mix')
    for bad in ('', 'Port "Playback"', 'x$(reboot)', 'Port\tPlayback', 'P' * 44):
        check(out(bad) == plain, f'{bad[:20]!r}: no stage, the PCMs byte for byte as before')


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
            elif verb in ('PAUSE', 'RESUME') and self.state != 'stopped':
                self.state = 'paused' if verb == 'PAUSE' else 'playing'
                self.send(f'STATE\tstate={self.state}\tpath={self.file}')
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

    def say(self, state):
        """paused or played on by itself (a headset's key)"""
        with self.lock:
            self.state = state
            self.send(f'STATE\tstate={state}\tpath={self.file}')

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


def wait_for(cond, seconds=5):
    until = time.time() + seconds
    while time.time() < until:
        if cond():
            return True
        time.sleep(0.05)
    return cond()


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
               ZYPEROS_MP4_SOCKET=str(msock), ZYPEROS_PORT_QUIET_CTL=str(tmp / 'quiet-control'))
    env.pop('ZYPEROS_HEADLESS', None)
    procs = []
    quiet_mark, quiet_ctl = flags / 'zyperos_port_quiet', tmp / 'quiet-control'

    def control():
        """what the stand-in for PICO-8's quiet control holds"""
        return quiet_ctl.read_text().strip() if quiet_ctl.exists() else None

    def start(name, args, no_control=False):
        if dsock.exists():
            dsock.unlink()
        log = open(tmp / f'{name}.log', 'w')
        e = dict(env)
        if no_control:
            e.pop('ZYPEROS_PORT_QUIET_CTL')
        p = subprocess.Popen([str(BINARY)] + args, env=e, stdout=log, stderr=subprocess.STDOUT)
        procs.append(p)
        return p

    def logged(name):
        return (tmp / f'{name}.log').read_text(errors='replace')

    def desk_up(p):
        ok = wait_for(lambda: dsock.exists() or p.poll() is not None, 10) and dsock.exists()
        if ok:
            ask(dsock, 'wait-frames 3')
        return ok

    def ended(p, seconds=8):
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
        check(desk_up(p), 'the launcher started')
        log = logged('start')
        check('mp4: took over 3 tracks, playing, at 1' in log, 'it takes the queue on')
        check(not hand.exists(), 'and removes the file')
        check('mp4: resumed' not in log, 'and resumes nothing: no power-off came')
        check(wait_for(lambda: '<connect>' in daemon.got, 3) and not daemon.plays(),
              'it talks to the daemon and sends no PLAY: the song plays on')
        daemon.end()
        check(wait_for(lambda: daemon.plays() == [str(card / TRACKS[1])], 3),
              f'an END: the next track, from the queue taken on ({daemon.plays()})')

        print('a PICO-8 cart opened:')
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(10)
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
        check(quiet_mark.exists() and quiet_mark.read_text().strip() == 'Port Playback Volume' and
              control() == '0', f'PICO-8\'s quiet control made, at silence as MP4 plays, and the mark '
              f'for launch.sh ({control()})')

        print('the follower beside it:')
        f = start('follow', ['--mp4-follow'])
        check(wait_for(lambda: 'mp4 follow: beside the program' in logged('follow'), 5) and
              'mp4: took over 3 tracks, playing, at 2' in logged('follow'), 'it takes the queue')
        check(wait_for(lambda: daemon.got.count('<connect>') == 2, 3), 'and talks to the daemon')
        check('quiet: PICO-8 quiet, MP4 plays' in logged('follow') and control() == '0',
              'PICO-8 quiet as MP4 plays')
        t0 = time.time()
        daemon.say('paused')
        check(wait_for(lambda: control() == '255', 2), 'MP4 paused (a headset\'s key): PICO-8 heard')
        check(time.time() - t0 < 0.5, f'at once ({time.time() - t0:.2f} s)')
        daemon.say('playing')
        check(wait_for(lambda: control() == '0', 2), 'MP4 played on: PICO-8 quiet again')
        daemon.end()
        check(wait_for(lambda: daemon.plays()[-1:] == [str(card / TRACKS[2])], 3) and control() == '0',
              'an END: the next track, PICO-8 still quiet')

        print('MENU over PICO-8, the follower stopped as portkeys stops it:')
        f.send_signal(signal.SIGSTOP)
        m = start('menu', ['--port-menu', 'PICO8', f'{card}/Roms/PICO-8/celeste.p8'])
        check(desk_up(m), 'the in-game menu is up')
        ask(dsock, 'tap select')
        ask(dsock, 'wait-frames 8')
        log = logged('menu')
        # The file says the song the launcher handed over, the second; the
        # follower went on to the third, which the daemon says it plays
        check('port menu: MP4 ready' in log and 'mp4: took over 3 tracks, playing, at 2' in log,
              'SELECT: MP4 made ready, the queue taken from the file')
        drawn = [l[len('fit: drawn: '):] for l in log.splitlines() if l.startswith('fit: drawn: ')]
        check('Home Again' in drawn and '3 / 3' in drawn, 'and Now Playing on the song the daemon plays')
        # L1 under 3 s into a song: the one before
        n = len(daemon.plays())
        ask(dsock, 'tap l1')
        ask(dsock, 'wait-frames 4')
        check(wait_for(lambda: daemon.plays()[n:] == [str(card / TRACKS[1])], 3),
              f'L1: the song before ({[g[:40] for g in daemon.got[-6:]]})')
        # A pauses it; PICO-8 is stopped under the menu, and the control is
        # left until the menu closes, as Diatom's QUIET is said at Continue
        ask(dsock, 'tap a')
        ask(dsock, 'wait-frames 6')
        check(wait_for(lambda: daemon.state == 'paused', 2) and control() == '0',
              'A in Now Playing pauses MP4; the control as it was while the menu is up')
        ask(dsock, 'press select')
        ask(dsock, 'wait-frames 3')
        ask(dsock, 'release select')
        ask(dsock, 'wait-frames 8')
        ask(dsock, 'tap b')
        rc = ended(m)
        check(rc == 10, f'SELECT let go closes MP4, and B is Continue ({rc})')
        h = hand.read_text() if hand.exists() else ''
        check(field(h, 'pos') == '1', f'the menu writes the queue back, on the song before ({field(h, "pos")})')
        check(control() == '255' and 'quiet: PICO-8 heard' in logged('menu'),
              'and PICO-8 heard, MP4 paused, written as the menu closes, before PICO-8 goes on')
        f.send_signal(signal.SIGCONT)
        check(wait_for(lambda: 'the queue again, from the in-game menu' in logged('follow'), 3),
              'the follower, let go on, takes the queue again')
        time.sleep(0.3)
        check(control() == '255', 'and leaves PICO-8 heard, MP4 still paused')
        n = len(daemon.plays())
        daemon.end()
        check(wait_for(lambda: daemon.plays()[n:] == [str(card / TRACKS[2])], 3),
              'and plays on from it: an END, the next track')
        check(wait_for(lambda: control() == '0', 2), 'MP4 playing again: PICO-8 quiet')
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
        check(desk_up(m), 'the in-game menu is up')
        n = len(daemon.got)
        ask(dsock, 'tap select')
        ask(dsock, 'wait-frames 6')
        ask(dsock, 'tap b')
        rc = ended(m)
        log = logged('menu-other')
        check(rc == 10 and 'MP4 ready' not in log and '<connect>' not in daemon.got[n:],
              f'SELECT opens nothing, and B is Continue ({rc})')
        check('quiet:' not in log, 'and writes no quiet control')

        print('PICO-8 opened where its quiet control cannot be made:')
        hand.write_text(hand_text(card, 0))
        daemon.say('playing')
        p = start('start-nocontrol', ['--device', 'pixel2'], no_control=True)
        check(desk_up(p), 'the launcher started')
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect(str(cdir / 'ctl.sock'))
        s.sendall(f'open\tpath={card}/Roms/PICO-8/celeste.p8\n'.encode())
        s.makefile().readline()
        s.close()
        check(ended(p) == 0, 'the launcher ends for it')
        check(not quiet_mark.exists() and 'quiet: no Port Playback Volume' in logged('start-nocontrol'),
              'no mark: launch.sh leaves PICO-8\'s chain as before, PICO-8 heard')
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
        eq_port(tmp)
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
