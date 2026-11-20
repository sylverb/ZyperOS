#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-pico8-keys: your PICO-8's hotkeys, Reset and buttons (DESIGN.md
11.55 and 11.61, src/portmenu.h, src/p8pad.h), what the launcher and
PICO-8.sh do of them. portkeys's half (SELECT's chords, the screenshot,
CTRL-R, Auto Off) is check-pkkeys's and check-portkeys-flow's.

1. sd/pixel2/PICO-8.sh, its card in a scratch folder and a stand-in
   pico8_64 that writes what it was started with: with no line in /tmp the
   pad's mapping is the one that ships (O on the B cap, X on the A cap); the
   launcher's line for this pad is taken as it is; a line of another pad, or
   with characters no mapping has, is not.
2. The real launcher (build-native/zyperos, make native) in desk mode's
   dummy window, a card with your PICO-8 on it, hotkeys.PICO8 and map.PICO8
   in its library:
   - a cart opened (px2-open's channel) leaves portkeys the hotkeys in force
     in Diatom's RUN form and the screenshot's folder and name
     (Screenshots/PICO-8/<the cart>), PICO-8.sh the pad's line from
     map.PICO8, and "Screenshot saved" in both themes with an empty square
     and its place after the pixels (NOTICE_SLOT);
   - the in-game menu over it (--port-menu PICO8 CART) has Reset between
     Controls and Quit; A asks "Reset the game?" with "Play since the last
     save is lost.", the cursor on Cancel: A there is the menu again; Reset
     ends it with PORT_MENU_RESET (13);
   - --reset (PORT_MENU_RESET_ARG, the hotkey's) asks the same question with
     no menu: Cancel 10, Reset 13;
   - Controls opens the buttons themselves, as the Controls tab over a game
     has them (Applies To, This Game, the A cap giving O, map.PICO8's): a
     button stepped is kept for the cart (map.PICO8.<cart>), B back writes the pad's new line and asks
     "Start the cart again?" with the buttons' line under it; Cancel is the
     menu again, and as the menu ends the hotkeys are written for portkeys
     again with the screenshot's folder kept; Start Again is
     PORT_MENU_RESTART (14) with the Display on in the restart file;
   - over OpenBOR there is no Reset row.
"""
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
GUID = '190043d2010000000221000000010000,gkd_pixel2_joypad,'
SHIPS = (GUID + 'a:b1,b:b0,x:b2,y:b3,back:b8,start:b9,leftshoulder:b4,rightshoulder:b5,dpup:b10,'
         'dpdown:b11,dpleft:b12,dpright:b13,lefttrigger:b6,righttrigger:b7,crc:d243,platform:Linux,')
SWAPPED = SHIPS.replace('a:b1,b:b0,', 'a:b0,b:b1,')
RESET, RESTART = 13, 14
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


# ---- 1. PICO-8.sh ---------------------------------------------------------

def script(tmp):
    d = tmp / 'p8sh'
    card, flags = d / 'card', d / 'flags'
    said = d / 'said'

    def run(line):
        shutil.rmtree(d, ignore_errors=True)
        for p in (card / 'Bios/PICO-8', card / 'Roms/PICO-8', card / 'Saves', flags, card / 'ZyperOS/pico8'):
            p.mkdir(parents=True, exist_ok=True)
        text = (ROOT / 'sd/pixel2/PICO-8.sh').read_text()
        check('/mnt/SDCARD' in text, 'PICO-8.sh has /mnt/SDCARD')
        sh = card / 'ZyperOS/pico8/PICO-8.sh'
        sh.write_text(text.replace('/mnt/SDCARD', str(card)))
        prog = card / 'Bios/PICO-8/pico8_64'
        prog.write_text(f'#!/bin/sh\nprintf "%s\\n" "$SDL_GAMECONTROLLERCONFIG" > "{said}"\n')
        prog.chmod(0o755)
        (card / 'Bios/PICO-8/pico8.dat').write_bytes(b'stand-in')
        (card / 'Roms/PICO-8/celeste.p8').write_text('pico-8 cartridge\n')
        if line is not None:
            (flags / 'zyperos-pico8-pad').write_text(line + '\n')
        env = dict(os.environ, ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_DISPLAY='stretch')
        env.pop('SDL_GAMECONTROLLERCONFIG', None)
        out = subprocess.run(['sh', str(sh), str(card / 'Roms/PICO-8/celeste.p8')], env=env,
                             stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=20)
        return (said.read_text().rstrip('\n') if said.exists() else None), out.stdout + out.stderr

    print('PICO-8.sh, the pad\'s mapping line:')
    got, log = run(None)
    check(got == SHIPS, f'no line in /tmp: the one that ships ({got})')
    got, log = run(SWAPPED)
    check(got == SWAPPED and 'the player\'s buttons' in log, 'the launcher\'s line for this pad: taken')
    got, log = run('030000005e0400008e02000014010000,xbox,a:b0,')
    check(got == SHIPS and 'not this pad\'s' in log, 'another pad\'s line: the one that ships')
    got, log = run(GUID + 'a:b1;reboot,b:b0,')
    check(got == SHIPS and 'no mapping line' in log, 'a line with what no mapping has: the one that ships')


# ---- 2. the launcher ------------------------------------------------------

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


def settings(path, put=None, key=None):
    c = sqlite3.connect(str(path))
    if put:
        for k, v in put.items():
            c.execute('INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)', (k, v))
        c.commit()
    r = c.execute('SELECT value FROM settings WHERE key=?', (key,)).fetchone() if key else None
    c.close()
    return r[0] if r else None


def dtov_slot(path):
    """The notice's size and the SLOT trailer's square, or None"""
    if not path.exists():
        return None
    b = path.read_bytes()
    if b[:4] != b'DTOV':
        return None
    w, h = b[4] | b[5] << 8, b[6] | b[7] << 8
    tr = b[8 + w * h * 4:]
    if len(tr) != 12 or tr[:4] != b'SLOT':
        return (w, h, None)
    return (w, h, tuple(tr[4 + i * 2] | tr[5 + i * 2] << 8 for i in range(4)))


def launcher(tmp):
    desk = module('desk', ROOT / 'tools/desk')
    rs = module('readme_shots', ROOT / 'tools/readme-shots.py')
    card, flags = tmp / 'card', tmp / 'flags'
    flags.mkdir(parents=True)
    rs.media_card(card)
    desk.card_sounds(card)
    env = desk.card_env(card, None)
    subprocess.run([str(BINARY), '--set', 'tour.done=1'], env=env, check=True, stdout=subprocess.DEVNULL)
    (card / 'Roms/PICO-8').mkdir(parents=True, exist_ok=True)
    (card / 'Roms/PICO-8/celeste.p8').write_text('pico-8 cartridge\n')
    (card / 'Bios/PICO-8').mkdir(parents=True, exist_ok=True)
    for f in ('pico8_64', 'pico8.dat'):
        (card / 'Bios/PICO-8' / f).write_bytes(b'stand-in')
    (card / 'ZyperOS/pico8').mkdir(parents=True, exist_ok=True)
    (card / 'ZyperOS/pico8/PICO-8.sh').write_text('#!/bin/sh\n')
    (card / 'Roms/OpenBOR').mkdir(parents=True, exist_ok=True)
    (card / 'Roms/OpenBOR/x.pak').write_bytes(b'')
    lib = next(iter(sorted((card / 'shared').rglob('library.db'))), None)
    check(lib is not None, 'the card\'s library')
    if lib is None:
        return
    # The player's hotkeys for PICO-8 and its buttons swapped, as the
    # Controls tab over a fake08 game keeps them (src/hkstore.h, remapstore.h)
    settings(lib, {'hotkeys.PICO8': '1;shot:y,reset:b,quit:x', 'map.PICO8': '1;a:b,b:a'})
    keys_file, pad_file = flags / 'zyperos-port-keys', flags / 'zyperos-pico8-pad'
    restart = flags / 'zyperos_port_restart'
    short = Path(tempfile.gettempdir())
    dsock = short / f'zyp8k-{os.getpid()}.sock'
    cdir = short / f'zyp8kc-{os.getpid()}'
    cdir.mkdir(mode=0o700, exist_ok=True)
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(dsock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(cdir))
    env.pop('ZYPEROS_HEADLESS', None)
    procs = []

    def start(name, args):
        if dsock.exists():
            dsock.unlink()
        log = open(tmp / f'{name}.log', 'w')
        p = subprocess.Popen([str(BINARY)] + args, env=env, stdout=log, stderr=subprocess.STDOUT)
        procs.append(p)
        ok = wait_for(lambda: dsock.exists() or p.poll() is not None, 10) and dsock.exists()
        if ok:
            ask(dsock, 'wait-frames 3')
        return p, ok

    def drawn(name):
        text = (tmp / f'{name}.log').read_text(errors='replace')
        return [l[len('fit: drawn: '):] for l in text.splitlines() if l.startswith('fit: drawn: ')]

    def keys(*names):
        for k in names:
            ask(dsock, f'tap {k}')
            ask(dsock, 'wait-frames 6')

    def ended(p, seconds=8):
        try:
            p.wait(timeout=seconds)
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            return None
        return p.returncode

    def lines(path):
        return dict(l.split('\t', 1) for l in path.read_text().splitlines() if '\t' in l) if path.exists() else {}

    cart = f'{card}/Roms/PICO-8/celeste.p8'
    try:
        print('a PICO-8 cart opened, the hand-over:')
        p, ok = start('shelf', ['--device', 'pixel2'])
        check(ok, 'the launcher started')
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(10)
        s.connect(str(cdir / 'ctl.sock'))
        s.sendall(f'open\tpath={cart}\n'.encode())
        said = s.makefile().readline().strip()
        s.close()
        check(said.startswith('ok') and 'core=run:ZyperOS/pico8/PICO-8.sh' in said,
              f'it opens on your PICO-8 ({said[:60]!r})')
        check(ended(p) == 0, 'the launcher ends for it')
        k = lines(keys_file)
        check(k.get('hotkeys') == 'save:r1,load:l1,faster:r2,slower:l2,shot:y,reset:b,quit:x',
              f'portkeys gets the hotkeys in force, in Diatom\'s RUN form ({k.get("hotkeys")})')
        shots = k.get('shots', '')
        check(shots.startswith(f'{card}/Screenshots/PICO-8/') and shots.rsplit('/', 1)[-1].lower().startswith('celeste'),
              f'and where a screenshot goes, the cart by its name ({shots})')
        pad = pad_file.read_text().rstrip('\n') if pad_file.exists() else ''
        check(pad == SWAPPED, f'PICO-8.sh gets the pad\'s line from map.PICO8 ({pad})')
        for theme in ('light', 'dark'):
            sl = dtov_slot(flags / f'zyperos-port-shot-{theme}.dtov')
            ok = sl is not None and sl[2] is not None
            if ok:
                w, h, (x, y, side, corner) = sl
                ok = side > 0 and x + side <= w and y + side <= h and 0 < corner <= side // 2
                # the square is the slab's own, no picture laid in it
                if ok:
                    b = (flags / f'zyperos-port-shot-{theme}.dtov').read_bytes()
                    mid = 8 + ((y + side // 2) * w + x + side // 2) * 4
                    edge = 8 + ((y + side // 2) * w + x + side + 4) * 4
                    ok = b[mid:mid + 4] == b[edge:edge + 4]
            check(ok, f'"Screenshot saved" in {theme}, with an empty square and its place ({sl})')

        print('the in-game menu over it, Reset:')
        m, ok = start('menu', ['--port-menu', 'PICO8', cart])
        seen = drawn('menu')
        check(ok and all(w in seen for w in ('Continue', 'Controls', 'Reset', 'Quit')) and
              seen.index('Controls') < seen.index('Reset') < seen.index('Quit'),
              'Reset, between Controls and Quit')
        rows = [w for w in seen if w in ('Continue', 'Display', 'Controls', 'Reset', 'Quit')]
        down = rows.index('Reset') if 'Reset' in rows else 3
        n = len(drawn('menu'))
        keys(*(['down'] * down), 'a')
        check(wait_for(lambda: 'Reset the game?' in drawn('menu')[n:] and
                       'Play since the last save is lost.' in drawn('menu')[n:], 3),
              'A asks first, as Reset Game\'s hotkey asks over a game')
        keys('a')
        check(m.poll() is None, 'A on the question\'s first stop is Cancel: the menu again')
        keys('a', 'up', 'a')
        rc = ended(m)
        check(rc == RESET, f'Reset ends the menu with PORT_MENU_RESET ({rc})')

        print('--reset, the hotkey\'s question alone:')
        m, ok = start('hot', ['--port-menu', 'PICO8', cart, '--reset'])
        seen = drawn('hot')
        check(ok and 'Reset the game?' in seen and 'Continue' not in seen, 'the question, and no menu')
        keys('a')
        rc = ended(m)
        check(rc == 10, f'Cancel: the game goes on ({rc})')
        m, ok = start('hot-yes', ['--port-menu', 'PICO8', cart, '--reset'])
        keys('up', 'a')
        rc = ended(m)
        check(rc == RESET, f'Reset: PORT_MENU_RESET ({rc})')

        print('Controls over it, a button changed:')
        keys_file.write_text(f'hotkeys\tnone\nshots\t{shots}\n')
        m, ok = start('ctl', ['--port-menu', 'PICO8', cart])
        seen = drawn('ctl')
        rows = [w for w in seen if w in ('Continue', 'Display', 'Controls', 'Reset', 'Quit')]
        n = len(seen)
        keys(*(['down'] * rows.index('Controls')), 'a')
        # O on the B cap, as map.PICO8 swaps them (the A cap's row says it)
        check(wait_for(lambda: 'Applies To' in drawn('ctl')[n:] and 'This Game' in drawn('ctl')[n:] and
                       'Button O' in drawn('ctl')[n:], 3),
              'A opens the buttons themselves: Applies To, This Game, the A cap giving O')
        # The first button's row, a step to the right
        keys('down', 'right')
        check(wait_for(lambda: settings(lib, key='map.PICO8.celeste.p8') is not None, 3),
              f'a button stepped is kept for the cart ({settings(lib, key="map.PICO8.celeste.p8")})')
        n = len(drawn('ctl'))
        keys('b')
        check(wait_for(lambda: 'Start the cart again?' in drawn('ctl')[n:] and
                       any('new buttons' in l for l in drawn('ctl')[n:]), 3),
              'B back asks to start PICO-8 again for the new buttons')
        pad = pad_file.read_text().rstrip('\n') if pad_file.exists() else ''
        check(pad.startswith(GUID) and pad != SWAPPED, f'and the pad\'s new line is written ({pad})')
        keys('a')
        check(m.poll() is None and not restart.exists(), 'Cancel: the menu again, no start again')
        keys('b')
        rc = ended(m)
        k = lines(keys_file)
        check(rc == 10 and k.get('hotkeys') == 'save:r1,load:l1,faster:r2,slower:l2,shot:y,reset:b,quit:x'
              and k.get('shots') == shots,
              f'as the menu ends the hotkeys are written again, the folder kept ({rc}, {k})')

        m, ok = start('ctl-yes', ['--port-menu', 'PICO8', cart])
        rows = [w for w in drawn('ctl-yes') if w in ('Continue', 'Display', 'Controls', 'Reset', 'Quit')]
        keys(*(['down'] * rows.index('Controls')), 'a', 'down', 'right', 'b', 'up', 'a')
        rc = ended(m)
        mode = settings(card / 'userdata/zyperos.db', key='display.PICO8') or 'stretch'
        check(rc == RESTART and restart.exists() and restart.read_text() == mode + '\n',
              f'Start Again: PORT_MENU_RESTART, the Display on in the restart file ({rc})')

        print('over OpenBOR:')
        m, ok = start('openbor', ['--port-menu', 'OPENBOR', f'{card}/Roms/OpenBOR/x.pak'])
        seen = drawn('openbor')
        check(ok and 'Continue' in seen and 'Reset' not in seen, 'no Reset row')
        keys('b')
        ended(m)
    finally:
        for p in procs:
            if p.poll() is None:
                p.kill()
                p.wait()
        shutil.rmtree(cdir, ignore_errors=True)
        if dsock.exists():
            dsock.unlink()


def main():
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-pico8-keys-'))
    try:
        script(tmp)
        if BINARY.is_file():
            launcher(tmp)
        else:
            check(False, 'build-native/zyperos is there (make native)')
    finally:
        if not fails:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print(f'check-pico8-keys: {len(fails)} failed (logs in {tmp})')
        return 1
    print('ok: your PICO-8\'s hotkeys, Reset and buttons, handed over and asked')
    return 0


if __name__ == '__main__':
    sys.exit(main())
