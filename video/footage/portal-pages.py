#!/usr/bin/env python3
"""The 1.0.6 video's File Transfer page (src/v106): the page in a computer's
browser, as tools/readme-shots.py takes portal.png, with what 1.0.6 added.

python3 video/footage/portal-pages.py

- xfer-pick.png: the NES folder with three games picked, the bar at the
  foot saying how many, with Move to folder and Delete
- xfer-locked.png: ROMs, a console's own folder's menu, which says why it
  cannot be moved, renamed or deleted

Both 1280x800, into video/public/img/. The page is res/web as the card has
it, served by build-native/portal-serve over a scratch card of freeware and
homebrew names (readme-shots.py's, and a few more on the NES), in headless
Chrome over the DevTools protocol (readme-shots.py's DevTools). The picks are
the page's own (pick(), openRowMenu()), as a click would make them.
"""
import base64
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import time
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('readme_shots', ROOT / 'tools/readme-shots.py')
rs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rs)
OUT = ROOT / 'video/public/img'

# More freeware NES homebrew, so the folder's list is a list
MORE_NES = [('Driar.nes', 40976), ('Lan Master.nes', 40976), ('Sir Ababol.nes', 40976), ('Concentration Room.nes', 40976),
            ('Super Tilt Bro.nes', 524304)]
PICKED = ['Blade Buster.nes', 'Lawn Mower.nes', 'Nova the Squirrel.nes']


def wait(dt, expr, what):
    for _ in range(100):
        r = dt.call('Runtime.evaluate', returnByValue=True, awaitPromise=True, expression=expr)
        if r.get('result', {}).get('value'):
            return
        time.sleep(0.1)
    raise RuntimeError(f'{what}: the page did not come up')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='zyperos-portal-') as temporary:
        base = Path(temporary)
        card, web, profile = base / 'card', base / 'web', base / 'chrome'
        rs.portal_card(card)
        rs.web_dir(web)
        for name, size in MORE_NES:
            with open(card / 'Roms/NES' / name, 'wb') as f:
                f.truncate(size)
        (card / 'Roms/NES/Homebrew').mkdir()
        subprocess.run(['make', '-s', 'build-native/portal-serve'], cwd=ROOT, check=True)
        server = subprocess.Popen([str(ROOT / 'build-native/portal-serve'), str(card), str(web), 'space=88.4/119.1'],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        browser = None
        try:
            pin, port = server.stdout.readline().split()
            origin = f'http://127.0.0.1:{port}'
            reply = urllib.request.urlopen(urllib.request.Request(origin + '/api/auth', data=pin.encode(), method='POST'))
            cookie = reply.headers['Set-Cookie'].split(';')[0].split('=', 1)
            browser = subprocess.Popen([rs.CHROME, '--headless=new', '--remote-debugging-port=0', f'--user-data-dir={profile}',
                                        '--no-first-run', '--no-default-browser-check', '--hide-scrollbars',
                                        '--disable-background-networking', '--disable-component-update', '--disable-sync',
                                        '--no-pings', 'about:blank'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for _ in range(100):
                if (profile / 'DevToolsActivePort').is_file() and (profile / 'DevToolsActivePort').read_text().strip():
                    break
                time.sleep(0.1)
            debug = (profile / 'DevToolsActivePort').read_text().split()[0]
            target = next(t for t in json.load(urllib.request.urlopen(f'http://127.0.0.1:{debug}/json/list')) if t['type'] == 'page')
            dt = rs.DevTools(target['webSocketDebuggerUrl'])
            dt.call('Network.enable')
            dt.call('Emulation.setEmulatedMedia', features=[{'name': 'prefers-color-scheme', 'value': 'light'},
                                                            {'name': 'prefers-reduced-motion', 'value': 'reduce'}])
            dt.call('Network.setCookie', name=cookie[0], value=cookie[1], url=origin + '/', httpOnly=True)
            dt.call('Emulation.setDeviceMetricsOverride', width=1280, height=800, deviceScaleFactor=1, mobile=False)
            up = ("document.fonts.ready.then(()=>document.readyState==='complete' && !document.getElementById('app').hidden"
                  " && document.querySelectorAll('#list li').length>0 && !document.getElementById('room').hidden)")

            def shoot(name):
                time.sleep(0.6)
                (OUT / name).write_bytes(base64.b64decode(dt.call('Page.captureScreenshot', format='png')['data']))
                print(OUT / name, flush=True)

            # The NES folder, three games picked as a click with Cmd picks them
            dt.call('Page.navigate', url=f'{origin}/#{urllib.parse.quote("roms/NES")}')
            wait(dt, up, 'NES')
            dt.call('Runtime.evaluate', expression=f"""
                for (const name of {json.dumps(PICKED)}) {{
                    const e = entries.find((x) => x.name === name);
                    pick(e, document.querySelector(`#list li[data-path="${{CSS.escape(e.path)}}"]`), true);
                }}""")
            shoot('xfer-pick.png')
            dt.call('Runtime.evaluate', expression='pickNone()')
            # ROMs: the Game Boy Advance's own folder's menu, from its ⋯
            dt.call('Page.navigate', url='about:blank')
            wait(dt, "location.href==='about:blank'", 'blank')
            dt.call('Page.navigate', url=f'{origin}/#{urllib.parse.quote("roms")}')
            wait(dt, up, 'ROMs')
            dt.call('Runtime.evaluate', expression="""
                (() => { const row = [...document.querySelectorAll('#list li.row')].find((li) => li.dataset.path === 'roms/Game Boy Advance');
                         row.scrollIntoView({ block: 'center' });
                         row.querySelector('.iconbtn').click(); })()""")
            shoot('xfer-locked.png')
        finally:
            if browser:
                browser.terminate()
                browser.wait()
            server.stdin.close()
            server.wait(timeout=10)


if __name__ == '__main__':
    main()
