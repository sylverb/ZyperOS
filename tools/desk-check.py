#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-desk: desk mode (src/device/desk.h, tools/desk), in SDL's dummy
window, so no window opens: the host launcher as the Pixel 2 and as the
Brick on tools/desk's sample card, driven over its control socket. Each one's
panel is its size (a shot is the panel's pixels, 640x480 or 1024x768), the
zoom and the buttons' bar change the window and come back in `state`, a
press is held until let go, a tap's frame is timed, and `quit` ends it.
build-native/zyperos must be built (make native)."""
import importlib.machinery
import importlib.util
import json
import os
import shutil
import socket
import struct
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


def desk_module():
    loader = importlib.machinery.SourceFileLoader('desk', str(ROOT / 'tools/desk'))
    spec = importlib.util.spec_from_loader('desk', loader)
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


def png_size(path):
    with open(path, 'rb') as f:
        head = f.read(24)
    return struct.unpack('>II', head[16:24]) if head[:8] == b'\x89PNG\r\n\x1a\n' else None


def run(desk, tmp, device, size):
    card = tmp / device
    desk.make_card(card, device, False)
    sock = Path(tempfile.gettempdir()) / f'zyperos-deskcheck-{os.getpid()}-{device}.sock'
    flags = tmp / f'{device}-flags'
    flags.mkdir()
    env = desk.card_env(card, None)
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', ZYPEROS_DESK_FORCE='1',
               ZYPEROS_DESK_SOCK=str(sock), ZYPEROS_FLAG_DIR=str(flags), ZYPEROS_CTL_DIR=str(tmp / f'{device}-ctl'))
    env.pop('ZYPEROS_HEADLESS', None)
    log = open(tmp / f'{device}.log', 'w')
    p = subprocess.Popen([str(BINARY), '--device', device], env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(200):
            if sock.exists():
                break
            if p.poll() is not None:
                break
            time.sleep(0.05)
        check(sock.exists(), f'{device}: the control socket is up')
        if not sock.exists():
            print((tmp / f'{device}.log').read_text()[-2000:])
            return
        st = ask(sock, 'state')
        check(st.get('device') == device and st.get('panel') == list(size),
              f'{device}: state says the {size[0]}x{size[1]} panel ({st})')
        shot = tmp / f'{device}.png'
        r = ask(sock, f'shot {shot}')
        check(r.get('ok') and png_size(shot) == size, f'{device}: a shot is the panel, {png_size(shot)}')
        ask(sock, 'press menu')
        held = ask(sock, 'state').get('held')
        ask(sock, 'release menu')
        check(held == ['menu'] and ask(sock, 'state').get('held') == [],
              f'{device}: a press is held until let go ({held})')
        lat = ask(sock, 'latency down')
        ms = lat.get('press_to_present_ms')
        check(lat.get('ok') and ms is not None and 0 <= ms < 1000, f'{device}: a tap is timed to its frame ({ms} ms)')
        ask(sock, 'zoom 2')
        ask(sock, 'bar 0')
        st = ask(sock, 'state')
        check(st.get('zoom') == 2 and st.get('bar') is False, f'{device}: zoom and the bar change ({st.get("zoom")}, {st.get("bar")})')
        bad = ask(sock, 'press nothing')
        check(bad.get('ok') is False and 'buttons' in bad.get('error', ''), f'{device}: an unknown button is refused')
        ask(sock, 'quit')
        try:
            p.wait(timeout=10)
        except subprocess.TimeoutExpired:
            pass
        check(p.poll() == 0, f'{device}: quit ends it (exit {p.poll()})')
    finally:
        if p.poll() is None:
            p.kill()
            p.wait()
        log.close()
        if sock.exists():
            sock.unlink()


def main():
    if not BINARY.is_file():
        print('check-desk: build-native/zyperos missing (make native)')
        return 1
    desk = desk_module()
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-deskcheck-'))
    try:
        run(desk, tmp, 'pixel2', (640, 480))
        run(desk, tmp, 'brick', (1024, 768))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f'desk: {"all passed" if not fails else f"{len(fails)} failed"}')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
