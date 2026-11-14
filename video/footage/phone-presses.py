#!/usr/bin/env python3
"""NES Player 2's play page on a phone, its buttons pressed by a finger.

python3 video/footage/phone-presses.py

The Reddit cut's Player 2 story shows the friend's phone playing: the play
page (res/web/play) served by the real p2d, in Chrome as tools/readme-shots.py
takes play-phone.png, with a touch on the d-pad and the A button, each picture
taken while the finger is down, so the page lights the keys itself. Writes
video/public/img/play-phone-<press>.png at the size of res/readme/play-phone.png;
the video puts Super Mario Bros. 3 in the picture box, as for play-phone.png.
"""
import base64
import importlib.util
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('readme_shots', ROOT / 'tools/readme-shots.py')
rs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rs)
OUT = ROOT / 'video/public/img'
PAGE = rs.PLAY_PAGES['play-phone']
# What a finger holds down for each picture: the d-pad's arm or a face button
PRESSES = {'right': ['#dpad [data-dir="right"]'], 'a': ['#a'], 'right-a': ['#dpad [data-dir="right"]', '#a'],
           'b': ['#b']}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='zyperos-phone-') as temporary:
        base = Path(temporary)
        web, profile, share, log = base / 'web', base / 'chrome', base / 'share', base / 'p2d.log'
        rs.web_dir(web)
        for font in ['menu.ttf', 'title.ttf']:
            shutil.copyfile(ROOT / 'res/fonts' / font, web / 'play' / font)
        subprocess.run(['make', '-s', 'build-native/p2d', 'build-native/p2d-feed'], cwd=ROOT, check=True)
        feed = subprocess.Popen([str(ROOT / 'build-native/p2d-feed'), '--share', str(share), '--fps', '60',
                                 '--frames', str(rs.PLAY_FRAMES)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        server = browser = None
        try:
            for _ in range(300):
                try:
                    head = share.read_bytes()[:16]
                except OSError:
                    head = b''
                if len(head) == 16 and struct.unpack('<I', head[12:16])[0] >= rs.PLAY_FRAMES:
                    break
                time.sleep(0.05)
            with open(log, 'wb') as out:
                server = subprocess.Popen([str(ROOT / 'build-native/p2d'), '--root', str(web), '--share', str(share),
                                           '--addr', '127.0.0.1', '--port', '0', '--lang', 'en'], stdout=out, stderr=out)
            port = None
            for _ in range(100):
                for line in log.read_text(errors='replace').splitlines():
                    if line.startswith('p2d: http on port '):
                        port = line.split()[-1]
                if port:
                    break
                time.sleep(0.1)
            browser = subprocess.Popen([rs.CHROME, '--headless=new', '--remote-debugging-port=0', f'--user-data-dir={profile}',
                                        '--no-first-run', '--no-default-browser-check', '--hide-scrollbars',
                                        '--disable-background-networking', '--disable-component-update', '--disable-sync',
                                        '--no-pings', 'about:blank'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for _ in range(100):
                if (profile / 'DevToolsActivePort').is_file() and (profile / 'DevToolsActivePort').read_text().strip():
                    break
                time.sleep(0.1)
            debug = (profile / 'DevToolsActivePort').read_text().split()[0]
            tab = json.load(urllib.request.urlopen(urllib.request.Request(
                f'http://127.0.0.1:{debug}/json/new?about:blank', method='PUT'), timeout=30))
            dt = rs.DevTools(tab['webSocketDebuggerUrl'])
            dt.call('Emulation.setEmulatedMedia', features=[{'name': 'prefers-color-scheme', 'value': 'light'},
                                                            {'name': 'prefers-reduced-motion', 'value': 'reduce'}])
            dt.call('Emulation.setDeviceMetricsOverride', width=PAGE['width'], height=PAGE['height'],
                    deviceScaleFactor=PAGE['scale'], mobile=True)
            dt.call('Emulation.setTouchEmulationEnabled', enabled=True, maxTouchPoints=5)
            dt.call('Page.navigate', url=f'http://127.0.0.1:{port}/play/')
            for _ in range(300):
                ready = dt.call('Runtime.evaluate', returnByValue=True, awaitPromise=True,
                                expression="document.fonts.ready.then(()=>document.readyState==='complete'"
                                           " && typeof window.p2==='object' && window.p2.frame>0"
                                           " && document.getElementById('game').dataset.state==='playing')")
                if ready.get('result', {}).get('value'):
                    break
                time.sleep(0.1)
            else:
                raise RuntimeError(f'the page did not come up as player 2: {log.read_text(errors="replace")!r}')
            time.sleep(0.5)
            for name, targets in PRESSES.items():
                points = []
                for n, sel in enumerate(targets):
                    box = dt.call('Runtime.evaluate', returnByValue=True,
                                  expression=f"(r=>[r.x+r.width/2,r.y+r.height/2])(document.querySelector({json.dumps(sel)})"
                                             ".getBoundingClientRect())")['result']['value']
                    points.append({'x': box[0], 'y': box[1], 'id': n})
                dt.call('Input.dispatchTouchEvent', type='touchStart', touchPoints=points)
                time.sleep(0.3)
                (OUT / f'play-phone-{name}.png').write_bytes(
                    base64.b64decode(dt.call('Page.captureScreenshot', format='png')['data']))
                dt.call('Input.dispatchTouchEvent', type='touchEnd', touchPoints=[])
                time.sleep(0.3)
                print(f'play-phone-{name}', flush=True)
            dt.s.close()
        finally:
            for proc in (browser, server, feed):
                if not proc:
                    continue
                proc.terminate()
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()


if __name__ == '__main__':
    main()
