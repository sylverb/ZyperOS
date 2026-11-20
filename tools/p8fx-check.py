#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-p8fx: your PICO-8's Shader and Border, all but the drawing
(src/p8fx.h; the drawing is check-p8fx-gl's, tools/p8fx-gl-check.py).

1. build-native/p8fx-check: the look file, the shaders and their steps, Next
   Shader on a scratch database, the picture out of PICO-8's preblit, the
   rect that covers the screen; and its list of shaders is the launcher's
   SHADERS (src/app.h) for the PICO-8 shelf on a GPU device, in order.
2. sd/pixel2/PICO-8.sh, on a card in a scratch folder with a stand-in
   pico8_64 that writes down its LD_PRELOAD: p8fx.so beside the script is
   loaded first, before one already set; with none, nothing is, and the log
   says so.
3. The real launcher in desk mode's dummy window (build-native/zyperos, make
   native), as portkeys starts it over the program (--port-menu PICO8 CART):
   the rows Continue, Display, On-Screen Stats, Border, Shader, Controls and
   Quit, Strength under an old screen; Shader stepped is shader.PICO8 kept and the look
   file written for the shim, Strength and Border the same; Your Own's
   picture made into its DTOV file; no Border where the mode covers the
   screen; over OpenBOR none of them. And a cart opened on your PICO-8 from
   the shelf writes the look file from the shelf's rows as the launcher
   hands over.
"""
import json
import os
import re
import shutil
import socket
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time
import importlib.machinery
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / 'build-native/zyperos'
CHECKER = ROOT / 'build-native/p8fx-check'
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


# ---- 1. the plain part ----------------------------------------------------

def plain():
    print('the look file, the shaders, Next Shader, the picture:')
    r = subprocess.run([str(CHECKER)], capture_output=True, text=True)
    for line in r.stdout.splitlines():
        if 'FAIL' in line:
            print(line)
    last = r.stdout.strip().splitlines()[-1:] or ['(nothing)']
    check(r.returncode == 0, f'build-native/p8fx-check: {last[0]}')
    listed = subprocess.run([str(CHECKER), '--list'], capture_output=True, text=True).stdout.split()
    text = (ROOT / 'src/app.h').read_text()
    rows = re.findall(r'\{\s*"([a-z-]+)",\s*S_[A-Z_]+,\s*(true|false),\s*(NULL|"[^"]*")\s*\}', text)
    want = [n for n, gpu, only in rows if only == 'NULL' or ' PICO8 ' in only]
    check(rows and listed == want, f'p8fx_shaders is SHADERS for PICO8 on a GPU device, in order ({listed})')


# ---- 2. PICO-8.sh ---------------------------------------------------------

def script(tmp):
    print('PICO-8.sh loads the library:')
    root = tmp / 'sh'
    card = root / 'card'
    zy = card / 'ZyperOS/pico8'
    for p in (card / 'Bios/PICO-8', card / 'Roms/PICO-8', root / 'bin', root / 'drm/card0-DSI-1', zy):
        p.mkdir(parents=True, exist_ok=True)
    (root / 'drm/card0-DSI-1/modes').write_text('480x640\n')
    stand = card / 'Bios/PICO-8/pico8_64'
    stand.write_text('#!/bin/sh\nprintf "%s" "${LD_PRELOAD-<unset>}" > "$P8_PRELOAD"\n')
    stand.chmod(0o755)
    (card / 'Bios/PICO-8/pico8.dat').write_text('stand-in')
    (card / 'Roms/PICO-8/x.p8').write_text('pico-8 cartridge\n')
    (root / 'bin/mount').write_text('#!/bin/sh\nexit 0\n')
    (root / 'bin/mount').chmod(0o755)
    text = (ROOT / 'sd/pixel2/PICO-8.sh').read_text()
    check('CARD=/mnt/SDCARD\n' in text and '/sys/class/drm/' in text, 'PICO-8.sh still has its CARD line and DRM folder')
    text = text.replace('CARD=/mnt/SDCARD\n', f"CARD='{card}'\n").replace('/sys/class/drm/', f'{root}/drm/')
    sh = zy / 'PICO-8.sh'
    sh.write_text(text)
    sh.chmod(0o755)
    got = root / 'preload'

    def run(extra=None):
        env = dict(os.environ, PATH=f'{root}/bin:' + os.environ['PATH'], ZYPEROS_DISPLAY='integer',
                   P8_PRELOAD=str(got))
        env.pop('LD_PRELOAD', None)
        if extra is not None:
            env['LD_PRELOAD'] = extra
        got.unlink(missing_ok=True)
        r = subprocess.run(['sh', str(sh), str(card / 'Roms/PICO-8/x.p8')], env=env, capture_output=True,
                           text=True, cwd=str(root))
        return (got.read_text() if got.exists() else None), r.stdout + r.stderr

    lib = zy / 'p8fx.so'
    lib.write_bytes(b'stand-in')
    pre, _ = run()
    check(pre == str(lib), f'p8fx.so beside the script is LD_PRELOAD ({pre})')
    pre, _ = run('/usr/lib/other.so')
    check(pre == f'{lib} /usr/lib/other.so', f'...first, before one already set ({pre})')
    lib.unlink()
    pre, out = run()
    check(pre == '<unset>', f'none beside it: no LD_PRELOAD ({pre})')
    check('no p8fx.so beside this script' in out, '...and the log says so')


# ---- 3. the launcher ------------------------------------------------------

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


def png(path, w, h):
    """A small PNG for Your Own, made with the standard library"""
    import zlib
    raw = b''.join(b'\x00' + b''.join(bytes((x * 255 // w, y * 255 // h, 90)) for x in range(w))
                   for y in range(h))

    def chunk(kind, data):
        c = struct.pack('>I', len(data)) + kind + data
        return c + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


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
    db = card / 'userdata/zyperos.db'

    def setting(key, value=None):
        c = sqlite3.connect(str(db))
        if value is None:
            c.execute('DELETE FROM settings WHERE key=?', (key,))
        else:
            c.execute('INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)', (key, value))
        c.commit()
        c.close()

    def value(key):
        try:
            c = sqlite3.connect(str(db))
            r = c.execute('SELECT value FROM settings WHERE key=?', (key,)).fetchone()
            c.close()
            return r[0] if r else None
        except sqlite3.Error as e:
            return f'<{e}>'

    look = flags / 'zyperos-pico8fx'
    picture = flags / 'zyperos-pico8-border.dtov'

    def look_lines():
        return dict(l.split(' ', 1) for l in look.read_text().splitlines()) if look.exists() else {}

    setting('display.PICO8', 'integer')
    short = Path(tempfile.gettempdir())
    dsock = short / f'zyp8fx-{os.getpid()}.sock'
    cdir = short / f'zyp8fxc-{os.getpid()}'
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

    def logged(name):
        return (tmp / f'{name}.log').read_text(errors='replace')

    def drawn(name):
        return [l[len('fit: drawn: '):] for l in logged(name).splitlines() if l.startswith('fit: drawn: ')]

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

    def order(seen, words):
        return all(w in seen for w in words) and [seen.index(w) for w in words] == sorted(seen.index(w) for w in words)

    cart = f'{card}/Roms/PICO-8/celeste.p8'
    try:
        print('the in-game menu over your PICO-8, at Sharp Pixels:')
        m, ok = start('menu', ['--port-menu', 'PICO8', cart])
        check(ok, 'it is up')
        seen = drawn('menu')
        check(order(seen, ['Continue', 'Display', 'On-Screen Stats', 'Border', 'Shader', 'Controls', 'Quit']),
              'Continue, Display, On-Screen Stats, Border, Shader, Controls, Quit: the Display tab\'s order')
        check('Strength' not in seen, 'no Strength while the shader is Off')
        keys('down', 'down', 'down', 'down', 'right')         # Shader: Off -> Sharp-Shimmerless
        check(wait_for(lambda: value('shader.PICO8') == 'shimmerless', 3), f'Right on Shader: shader.PICO8 kept '
              f'({value("shader.PICO8")})')
        check(look_lines().get('shader') == 'shimmerless', f'...and the look file says it ({look_lines()})')
        n = len(drawn('menu'))
        keys('right')                                         # Sharp-Shimmerless -> CRT
        check(look_lines().get('shader') == 'crt' and value('shader.PICO8') == 'crt', 'Right again: CRT')
        check(wait_for(lambda: 'Strength' in drawn('menu')[n:], 3), '...and Strength under it')
        keys('down', 'left')                                  # Strength 100 -> 90
        check(value('shader_strength.PICO8') == '90' and look_lines().get('strength') == '90',
              f'Left on Strength: 90%, kept and in the file ({value("shader_strength.PICO8")}, {look_lines()})')
        keys('up', 'up', 'right')                             # Border: Off -> Blur
        check(value('border.PICO8') == 'blur' and look_lines().get('border') == 'blur',
              f'Right on Border: Blur, kept and in the file ({value("border.PICO8")}, {look_lines()})')
        keys('b')
        check(ended(m) == 10, 'B is Continue')

        print('Your Own, a picture in Borders/PICO-8:')
        (card / 'Borders/PICO-8').mkdir(parents=True, exist_ok=True)
        png(card / 'Borders/PICO-8/frame.png', 64, 48)
        m, ok = start('own', ['--port-menu', 'PICO8', cart])
        keys('down', 'down', 'down', 'right')                 # Border: Blur -> Your Own
        lines = look_lines()
        check(value('border.PICO8') == 'own' and lines.get('border') == 'picture' and
              lines.get('border_file') == str(picture), f'Your Own: the look file names its picture ({lines})')
        head = picture.read_bytes()[:8] if picture.exists() else b''
        size = struct.unpack('<HH', head[4:8]) if len(head) == 8 else None
        # The panel's: the Pixel 2's 640x480 on the device, the desk's own
        # 1024x768 here, where --port-menu takes no --device
        check(head[:4] == b'DTOV' and size in ((640, 480), (1024, 768)) and
              picture.stat().st_size == 8 + size[0] * size[1] * 4, f'...a DTOV file the panel\'s size ({size})')
        keys('b')
        ended(m)

        print('at Full Screen, which covers the screen:')
        setting('display.PICO8', 'stretch')
        m, ok = start('stretch', ['--port-menu', 'PICO8', cart])
        seen = drawn('stretch')
        check(ok and 'Shader' in seen and 'Border' not in seen, 'Shader, and no Border')
        keys('b')
        ended(m)

        print('over OpenBOR:')
        m, ok = start('openbor', ['--port-menu', 'OPENBOR', f'{card}/Roms/OpenBOR/x.pak'])
        seen = drawn('openbor')
        check(ok and 'Continue' in seen and 'Shader' not in seen and 'Border' not in seen,
              'no Shader and no Border')
        keys('b')
        ended(m)

        print('a cart opened on your PICO-8 from the shelf:')
        setting('display.PICO8', 'integer')
        setting('shader.PICO8', 'lcd')
        setting('shader_strength.PICO8', '40')
        setting('border.PICO8', 'blur')
        look.unlink(missing_ok=True)
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
        lines = look_lines()
        check(lines.get('shader') == 'lcd' and lines.get('strength') == '40' and lines.get('border') == 'blur',
              f'the look file is the shelf\'s rows ({lines})')
        check('p8fx: PICO-8: shader lcd at 40%, border blur' in logged('shelf'), 'and the log says it')
    finally:
        for p in procs:
            if p.poll() is None:
                p.kill()
                p.wait()
        shutil.rmtree(cdir, ignore_errors=True)
        if dsock.exists():
            dsock.unlink()


def main():
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-p8fx-'))
    try:
        if CHECKER.is_file():
            plain()
        else:
            check(False, 'build-native/p8fx-check is there')
        script(tmp)
        if BINARY.is_file():
            launcher(tmp)
        else:
            check(False, 'build-native/zyperos is there (make native)')
    finally:
        if not fails:
            shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        print(f'check-p8fx: {len(fails)} failed (logs in {tmp})')
        return 1
    print('ok: your PICO-8\'s Shader and Border, kept, written for the shim and loaded with it')
    return 0


if __name__ == '__main__':
    sys.exit(main())
