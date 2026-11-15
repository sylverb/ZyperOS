#!/usr/bin/env python3
"""The File Transfer page, driven in a browser: make check-portal.

    make build-native/portal-serve
    python3 tools/portal-check.py [--chrome PATH]

build-native/portal-serve serves the real page (res/web) and the real routes
(src/hare.c) over a scratch card of freeware names, as tools/readme-shots.py
does for its pictures, and headless Chrome runs the page over the DevTools
protocol. What it holds, each a thing I asked for on 2026-11-06:

  - every folder wears its pixel art (the shelf's, served at /icons/KEY.png):
    a console its own, Atomiswave too though it has no games, the card's
    folders in the sidebar theirs, Menu Music the cassette; a folder of
    ROMs that is no system, such as ROGUE's old BGM, the plain one
  - dot folders (.media) are not listed, and are when Settings says so
  - a console's .media, dropped onto its folder or in a folder picked,
    goes up with its covers, and nothing else hidden does (2026-11-07)
  - a batch that does not fit the card is asked about before a byte goes,
    with the files that fit offered in their order
  - the uploads show the time left for all of them, once there is a speed
  - a file of an exact multiple of 128 KiB, which Safari over the cable
    never hears the answer to, goes with the page's trailer and lands to the
    byte: an upload, a cover, an update's file (2026-11-07)
  - the handheld's "stop after this file": the file on its way finishes, the
    rest stay on the computer and the panel says so
  - Settings: the update over the cable, off on an image with no release key,
    and with one, the files dropped, sent, checked and installed
  - the page in the handheld's language (MENU > Language, 2026-11-07): every
    word from its strings, the name the cable's, sizes the language's way
  - a row dragged onto a folder's row or a crumb above moves there on the
    card, the rows picked with it too (Shift, Ctrl or Cmd with a press), by
    mouse or by a finger held still a moment, a disc set's folder whole; a
    name already there is refused and said (2026-11-13)
  - picking several (1.0.6): a finger held still picks a row, a press then
    picks or unpicks (a folder too, without opening it), Select all, Ctrl+A
    and Escape; Move to folder walks from the folder above, off for ROMs
    itself and for where they are, New folder makes one and opens it, and
    the move lands; Delete asks once for all of them
  - the page named after its link, as the handheld's screen is: USB File
    Transfer over the cable and Wi-Fi File Transfer over Wi-Fi, in its tab,
    on the PIN screen and over the folders, and never a name the browser
    kept from the other link
  - a drop in the middle of an upload (2026-11-14: a 517 MB disc lost
    whole to a few seconds without Wi-Fi): the file waits for the handheld,
    says how much it kept and, after a while, the address to check, then
    goes on from there and lands to the byte; Stop sending lets the handheld
    go of its part; ten minutes with no answer give up, and Try again goes
    on from the part; a page loaded again, with the same file picked again,
    goes on from it too

No device is touched and nothing is fetched from the internet: the release
GitHub would answer is given to the page. Chrome is needed; without it the
check says so and fails, as a check that cannot run is not a pass.
"""
import argparse
import base64
import contextlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('readme_shots', ROOT / 'tools/readme-shots.py')
rs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rs)

failures = 0


def check(ok, what):
    global failures
    print(('  ok: ' if ok else '  FAIL: ') + what, flush=True)
    if not ok:
        failures += 1


@contextlib.contextmanager
def scratch(prefix):
    """A temp folder, removed afterwards however long Chrome takes to let go:
    its helper processes can still write into the profile here for a moment
    after the browser has exited, and TemporaryDirectory's removal then
    failed the check (seen under make check's parallel run, 2026-11-06)"""
    path = tempfile.mkdtemp(prefix=prefix)
    try:
        yield path
    finally:
        for _ in range(50):
            shutil.rmtree(path, ignore_errors=True)
            if not os.path.exists(path):
                break
            time.sleep(0.1)


class Page:
    """A served page in a headless Chrome tab"""

    def __init__(self, chrome, base):
        self.profile = base / 'chrome'
        self.browser = subprocess.Popen(
            [chrome, '--headless=new', '--remote-debugging-port=0', f'--user-data-dir={self.profile}',
             '--no-first-run', '--no-default-browser-check', '--disable-background-networking',
             '--disable-component-update', '--disable-sync', '--no-pings',
             # at the priority of a page in front, as tools/play-check.py's
             # CHROME_FLAGS says: covered or in the background, Chrome turns
             # its renderer down, and the page's timed steps came late
             '--disable-renderer-backgrounding', '--disable-background-timer-throttling',
             '--disable-backgrounding-occluded-windows', 'about:blank'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            port = self.profile / 'DevToolsActivePort'
            if port.is_file() and port.read_text().strip():
                break
            time.sleep(0.1)
        debug = (self.profile / 'DevToolsActivePort').read_text().split()[0]
        target = next(t for t in json.load(urllib.request.urlopen(f'http://127.0.0.1:{debug}/json/list'))
                      if t['type'] == 'page')
        self.dt = rs.DevTools(target['webSocketDebuggerUrl'])
        self.dt.call('Network.enable')
        self.dt.call('Emulation.setDeviceMetricsOverride', width=1280, height=800, deviceScaleFactor=1, mobile=False)
        self.dt.call('Emulation.setEmulatedMedia', features=[{'name': 'prefers-reduced-motion', 'value': 'reduce'}])

    def js(self, expr):
        r = self.dt.call('Runtime.evaluate', expression=expr, returnByValue=True, awaitPromise=True)
        if 'exceptionDetails' in r:
            raise RuntimeError(f'{expr[:60]}...: {r["exceptionDetails"]}')
        return r.get('result', {}).get('value')

    def until(self, expr, seconds=10):
        end = time.time() + seconds
        while time.time() < end:
            if self.js(expr):
                return True
            time.sleep(0.1)
        return False

    def close(self):
        self.browser.terminate()
        self.browser.wait()


class Server:
    """build-native/portal-serve over a card, with its stop question on stdin"""

    def __init__(self, card, web, *args):
        self.p = subprocess.Popen([str(ROOT / 'build-native/portal-serve'), str(card), str(web), *args],
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        self.pin, port = self.p.stdout.readline().split()
        self.origin = f'http://127.0.0.1:{port}'

    def say(self, line):
        self.p.stdin.write(line + '\n')
        self.p.stdin.flush()

    def stop(self):
        try:
            self.p.stdin.close()
        except BrokenPipeError:
            pass
        self.p.wait(timeout=10)


def fresh(page, url):
    """A new page at url: by way of about:blank, waited for, or Chrome holds
    the second navigation behind the first. An upload the page is in the middle
    of is let go first: the page asks before it is left (beforeunload), and
    the question would hold the navigation."""
    page.js("typeof sending === 'undefined' || (sending = false)")
    page.dt.call('Page.navigate', url='about:blank')
    page.until("location.href === 'about:blank'")
    page.dt.call('Page.navigate', url=url)


def signed_in(page, server, path='roms'):
    fresh(page, f'{server.origin}/#{path}')
    page.until("document.readyState === 'complete' && typeof sendPin === 'function'")
    page.js(f"sendPin({json.dumps(server.pin)})")
    return page.until("!document.getElementById('app').hidden && "
                      "(document.querySelectorAll('#list li').length > 0 || !document.getElementById('settings').hidden)")


RELEASE = {'tag_name': 'v0.4.1', 'assets': [
    {'name': 'ZyperOS-pixel2-update.txt', 'size': 18000, 'browser_download_url': 'https://example.invalid/a'},
    {'name': 'ZyperOS-v0.4.1-pixel2-update.zpk', 'size': 38000000, 'browser_download_url': 'https://example.invalid/b'}]}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--chrome', default=rs.CHROME)
    args = ap.parse_args()
    if not Path(args.chrome).is_file():
        print(f'FAIL: no browser at {args.chrome} (--chrome PATH)')
        return 1
    # make check-portal has just built it; run alone, this builds it once
    if not (ROOT / 'build-native/portal-serve').is_file():
        subprocess.run(['make', '-s', 'build-native/portal-serve'], cwd=ROOT, check=True)
    with scratch('zyperos-portal-check-') as temporary:
        base = Path(temporary)
        card, web = base / 'card', base / 'web'
        rs.portal_card(card)
        # ROGUE OS's menu music folder, an ordinary folder since 1.0.5
        (card / 'Roms/BGM').mkdir(exist_ok=True)
        rs.web_dir(web)
        (card / 'Roms/NES/.media').mkdir()
        page = Page(args.chrome, base)
        try:
            print('the folders, their art, and what is hidden:')
            server = Server(card, web, 'usb', 'update=off')
            check(signed_in(page, server), 'the page came up past the PIN')
            got = page.js("""(() => { const row = (n) => [...document.querySelectorAll('#list li')]
                .find((li) => li.querySelector('.name').textContent === n);
                const src = (n) => { const r = row(n); const i = r && r.querySelector('.sico img');
                    return i ? i.getAttribute('src') : null; };
                return { nes: src('NES'), aw: src('Atomiswave'), bgm: src('BGM'), snes: src('SNES'),
                         tile: row('NES') && row('NES').querySelector('.sico').style.getPropertyValue('--sys'),
                         places: [...document.querySelectorAll('#places button')].map((b) =>
                             b.querySelector('img') ? b.querySelector('img').getAttribute('src')
                                                    : [...b.classList].find((c) => c.startsWith('i-'))) }; })()""")
            check(got['nes'] == '/icons/NES.png' and got['snes'] == '/icons/SNES.png', f"consoles wear the shelf's art: {got}")
            check(got['aw'] == '/icons/AW.png', 'Atomiswave, with no games, has its art')
            check(got['bgm'] == '/icons/FOLDER.png', f"Roms/BGM is an ordinary folder: {got['bgm']}")
            check(bool(got['tile']), "a console's tile is in its color")
            check(got['places'] == ['/icons/ROMS.png', '/icons/MUSIC.png', '/icons/BOOKS.png', '/icons/VIDEOS.png',
                                    '/icons/BGM.png', '/icons/BIOS.png', '/icons/SAVES.png', '/icons/STATES.png'],
                  f"the card's folders wear theirs, Music its vinyl album, Menu Music the cassette: {got['places']}")
            # Lazy, so the ones on screen: those below come as they scroll in
            check(page.until("[...document.querySelectorAll('#list .sico img, #places img')]"
                             ".filter((i) => i.getBoundingClientRect().top < innerHeight)"
                             ".every((i) => i.complete && i.naturalWidth > 0)"),
                  'every picture on screen came')
            page.js("go('roms/NES', 'none')")
            check(page.until("cwd === 'roms/NES'") and
                  not page.js("[...document.querySelectorAll('#list .name')].some((n) => n.textContent === '.media')"),
                  '.media is not listed')
            page.js("document.getElementById('showhidden').click(); go('roms/NES', 'none')")
            check(page.until("[...document.querySelectorAll('#list .name')].some((n) => n.textContent === '.media')"),
                  'Show hidden folders lists it')
            page.js("document.getElementById('showhidden').click()")

            print("a console's .media, dropped or picked, and nothing else hidden:")
            # A picked folder as the Upload folder picker hands it over, each
            # file with its path inside the folder, and a dropped one as the
            # entries API walks it: a Mac's litter beside the games and inside
            # .media, a hidden folder, and the covers in .media
            page.js("""window.__picked = (() => {
                const f = (path, text) => { const x = new File([text], path.split('/').pop());
                    Object.defineProperty(x, 'webkitRelativePath', { value: path }); return x; };
                return picked([f('Pick/Game.nes', 'GAME'), f('Pick/.DS_Store', 'x'), f('Pick/._Game.nes', 'x'),
                    f('Pick/.media/Game.png', 'COVER1'), f('Pick/.media/.DS_Store', 'x'),
                    f('Pick/.media/._Game.png', 'x'), f('Pick/.hidden/x.png', 'x'),
                    f('Pick/.media/.thumbs/y.png', 'x')]); })()""")
            got = page.js("[window.__picked.dirs, window.__picked.files.map((f) => f.rel + '/' + f.file.name)]")
            check(got == [['Pick', 'Pick/.media'], ['Pick/Game.nes', 'Pick/.media/Game.png']],
                  f'a picked folder keeps .media and leaves the rest hidden out: {got}')
            page.js("enqueueTree(window.__picked, 'roms/NES')")
            done = "!sending && queue.length > 0 && queue.every((j) => j.state === 'done')"
            check(page.until(done), 'the picked folder went up: ' +
                  str(page.js("queue.map((j) => j.file.name + ' ' + j.state + ' ' + (j.why || ''))")))
            page.js("""(() => {
                const file = (name, text) => ({ name, isFile: true, isDirectory: false,
                    file: (ok) => ok(new File([text], name)) });
                const dir = (name, kids) => ({ name, isFile: false, isDirectory: true, createReader: () => {
                    let given = false;
                    return { readEntries: (ok) => { ok(given ? [] : kids); given = true; } }; } });
                const media = dir('.media', [file('Contra.png', 'COVER2'), file('.DS_Store', 'x'),
                    file('._Contra.png', 'x'), dir('.Trashes', [file('z.png', 'x')])]);
                dropped({ items: [{ webkitGetAsEntry: () => media }], files: [] }, 'roms/NES'); })()""")
            check(page.until(done + " && queue.some((j) => j.file.name === 'Contra.png')"),
                  "a console's .media dropped onto its folder went up: " +
                  str(page.js("queue.map((j) => j.file.name + ' ' + j.state + ' ' + (j.why || ''))")))
            nes = card / 'Roms/NES'
            check((nes / 'Pick/Game.nes').read_bytes() == b'GAME' and
                  (nes / 'Pick/.media/Game.png').read_bytes() == b'COVER1',
                  "the picked folder's game and its cover in its .media")
            check((nes / '.media/Contra.png').read_bytes() == b'COVER2', "the dropped cover in NES's .media")
            hidden = sorted(str(p.relative_to(card)) for p in nes.rglob('*')
                            if any(part.startswith('.') and part != '.media' for part in p.relative_to(nes).parts))
            check(not hidden, f'nothing hidden but .media reached the card: {hidden}')
            page.js("fresh()")

            print('files of an exact multiple of 128 KiB, and the rest:')
            # Safari never hears the answer to a PUT of such a size, so the
            # page sends it with a trailer the handheld takes off: here, that
            # it does, for the queue and for a cover, and the card has each
            # file to the byte. Chrome hears every answer either way.
            page.js("""(() => {
                window.__puts = [];
                const open = XMLHttpRequest.prototype.open;
                XMLHttpRequest.prototype.open = function (m, url) { window.__puts.push(url); return open.apply(this, arguments); };
                const f = (name, n) => new File([new Uint8Array(n).map((_, i) => i * 7 % 251)], name);
                enqueue([f('k128.bin', 131072), f('k256.bin', 262144), f('k0.bin', 0), f('k1.bin', 1000)], 'roms/NES');
                window.__cover = putFile('roms/NES/.media/k128.png', f('k128.png', 131072)).then(() => 'ok', (e) => e.message);
            })()""")
            check(page.until(done + " && queue.length === 4", 20) and page.js("window.__cover") == 'ok',
                  'they went up: ' + str(page.js("queue.map((j) => j.file.name + ' ' + j.state + ' ' + (j.why || ''))")))
            puts = page.js("window.__puts")
            check(len(puts) == 4 and all(u.endswith('&trailer=16') == ('k1.bin' not in u) for u in puts),
                  f'the multiples of 128 KiB, an empty one too, went with the trailer, and only they: {puts}')
            pattern = bytes(i * 7 % 251 for i in range(262144))
            sizes = {name: (nes / name).read_bytes() for name in ('k128.bin', 'k256.bin', 'k0.bin', 'k1.bin')}
            check(sizes == {'k128.bin': pattern[:131072], 'k256.bin': pattern, 'k0.bin': b'', 'k1.bin': pattern[:1000]},
                  'each is on the card to the byte: ' + str({k: len(v) for k, v in sizes.items()}))
            check((nes / '.media/k128.png').read_bytes() == pattern[:131072], 'and the cover, to the byte')
            check(not list(nes.rglob('*.part')), 'no part-file is left')
            page.js("fresh()")

            print('room on the card:')
            page.js("go('roms/NES', 'none')")
            page.js("""(() => { const f = (n, s) => { const x = new File(['x'], n); Object.defineProperty(x, 'size', { value: s }); return x; };
                window.__fit = fitting([f('a.nes', 1e15), f('b.nes', 1000), f('c.nes', 2000)]); })()""")
            check(page.until("!document.getElementById('dialog').hidden"), 'a batch that does not fit is asked about')
            words = page.js("[document.getElementById('dlghead').textContent, document.getElementById('dlgyes').textContent]")
            check(words[0] == 'Not enough room on the card' and words[1] == 'Send the 2 that fit',
                  f'it offers the ones that fit: {words}')
            page.js("document.getElementById('dlgyes').click()")
            check(page.js("window.__fit.then((f) => f.map((x) => x.name).join(','))") == 'b.nes,c.nes',
                  'the files that fit, in their order')

            print('the time left, and the handheld stopping after the file on its way:')
            page.dt.call('Network.emulateNetworkConditions', offline=False, latency=0,
                         downloadThroughput=-1, uploadThroughput=262144)
            page.js("enqueue([new File([new Uint8Array(1500000)], 'Lizard.nes'), "
                    "new File([new Uint8Array(400000)], 'Micro Mages.nes'), "
                    "new File([new Uint8Array(400000)], 'Alter Ego.nes')])")
            check(page.until("/ left · about \\d+ s$/.test(document.getElementById('queueleft').textContent)", 8),
                  'the queue says what is left and the time: ' +
                  str(page.js("document.getElementById('queueleft').textContent")))
            server.say('after')
            check(page.until("document.getElementById('queuehead').textContent === '1 sent, 2 not sent'", 20),
                  'the file on its way finished and the rest stayed: ' +
                  str(page.js("document.getElementById('queuehead').textContent")))
            check('closing' in (page.js("document.getElementById('queueleft').textContent") or ''),
                  'the panel says the handheld closed it')
            check((card / 'Roms/NES/Lizard.nes').is_file() and not (card / 'Roms/NES/Micro Mages.nes').exists(),
                  'on the card: the first file, whole, and nothing of the rest')
            page.dt.call('Network.emulateNetworkConditions', offline=False, latency=0,
                         downloadThroughput=-1, uploadThroughput=-1)
            server.stop()

            print('Settings, and the update over the cable:')
            server = Server(card, web, 'usb', 'update=off')
            check(signed_in(page, server, 'settings'), 'Settings opened from its place in the address')
            check(page.until("/no release key/.test(document.getElementById('updbody').textContent)"),
                  'an image with no release key says updates are off, and why')
            server.stop()
            server = Server(card, web, 'usb', 'update=on')
            fresh(page, f'{server.origin}/#settings')
            page.until("document.readyState === 'complete' && typeof sendPin === 'function'")
            page.js("window.ZYPEROS_RELEASES = 'data:application/json,' + encodeURIComponent(" +
                    json.dumps(json.dumps(RELEASE)) + ")")
            page.js(f"sendPin({json.dumps(server.pin)})")
            check(page.until("/0\\.4\\.1 is out/.test(document.getElementById('updbody').textContent)"),
                  'the newest release is offered, with the files it takes')
            # The pack an exact 256 KiB: the update's files go with the
            # trailer too, and land without it
            page.js("upTake([new File(['zyperos-update 1\\n'], 'ZyperOS-pixel2-update.txt'), "
                    "new File([new Uint8Array(262144)], 'ZyperOS-v0.4.1-pixel2-update (1).zpk')])")
            check(page.until("/is ready/.test(document.getElementById('updbody').textContent)", 30),
                  'sent, checked and installed: ' + str(page.js("document.getElementById('updbody').innerText.split('\\n')[0]")))
            cable = card / '.userdata/pixel2/update/cable'
            check((cable / 'ZyperOS-pixel2-update.txt').is_file() and
                  (cable / 'ZyperOS-v0.4.1-pixel2-update.zpk').read_bytes() == bytes(262144),
                  'the files landed under their own names, a browser\'s "(1)" off, the 256 KiB one to the byte')
            server.stop()

            print('dragging moves files on the card, by mouse and by finger:')
            nes = card / 'Roms/NES'
            (nes / 'Hacks').mkdir(exist_ok=True)
            (nes / 'Set').mkdir(exist_ok=True)
            (nes / 'Set/Set.m3u').write_text('a.chd\n')
            for n in ('A Drag Me.nes', 'A Pick 1.nes', 'A Pick 2.nes', 'A Twice.nes'):
                (nes / n).write_bytes(b'x')
            (nes / 'Hacks/A Twice.nes').write_bytes(b'other')
            server = Server(card, web, 'usb', 'update=off')
            check(signed_in(page, server, 'roms/NES'), 'the page came up on NES')
            page.js("go('roms/NES', 'none')")
            page.until("cwd === 'roms/NES' && document.querySelectorAll('#list li').length > 3")

            def at(sel):
                r = page.js(f"(() => {{ const e = {sel}; if (!e) return null; const b = e.getBoundingClientRect();"
                            " return [b.left + b.width / 2, b.top + b.height / 2]; })()")
                return r

            def row(name):
                return f"[...document.querySelectorAll('#list li')].find((li) => li.querySelector('.name').textContent === {json.dumps(name)})"

            def mouse_drag(src, dst, mods=0):
                (x0, y0), (x1, y1) = at(row(src)), dst
                page.dt.call('Input.dispatchMouseEvent', type='mouseMoved', x=x0, y=y0)
                page.dt.call('Input.dispatchMouseEvent', type='mousePressed', x=x0, y=y0, button='left', clickCount=1, modifiers=mods)
                for k in range(1, 9):
                    page.dt.call('Input.dispatchMouseEvent', type='mouseMoved', x=x0 + (x1 - x0) * k / 8,
                                 y=y0 + (y1 - y0) * k / 8, button='left', buttons=1)
                page.dt.call('Input.dispatchMouseEvent', type='mouseReleased', x=x1, y=y1, button='left', clickCount=1)

            # The test's rows first among the files, so all are in the window
            page.js("scrollTo(0, 0)")
            # A mouse: a row onto a folder's row
            mouse_drag('A Drag Me.nes', at(row('Hacks')))
            check(page.until("!document.querySelector('.ghost')") and
                  page.until(f"!{row('A Drag Me.nes')}") and (nes / 'Hacks/A Drag Me.nes').is_file() and
                  not (nes / 'A Drag Me.nes').exists(), 'a row dragged onto a folder moved there')
            check(page.js("cwd") == 'roms/NES', 'and the page stayed where it was')
            # Two picked with Cmd/Ctrl, the one dragged takes the other with it
            for n in ('A Pick 1.nes', 'A Pick 2.nes'):
                x, y = at(row(n))
                page.dt.call('Input.dispatchMouseEvent', type='mousePressed', x=x, y=y, button='left', clickCount=1, modifiers=8)
                page.dt.call('Input.dispatchMouseEvent', type='mouseReleased', x=x, y=y, button='left', clickCount=1, modifiers=8)
            check(page.js("document.querySelectorAll('#list li.sel').length") == 2, 'Shift and a press pick two rows')
            mouse_drag('A Pick 1.nes', at(row('Hacks')))
            check(page.until(f"!{row('A Pick 2.nes')}") and (nes / 'Hacks/A Pick 1.nes').is_file() and
                  (nes / 'Hacks/A Pick 2.nes').is_file(), 'the picked rows went together')
            # A name already in the folder: refused, said, and both stay
            mouse_drag('A Twice.nes', at(row('Hacks')))
            check(page.until("/Twice\\.nes is already in Hacks/.test(document.getElementById('toast').textContent)"),
                  'a name already in the folder is refused and said: ' +
                  str(page.js("document.getElementById('toast').textContent")))
            check((nes / 'A Twice.nes').is_file() and (nes / 'Hacks/A Twice.nes').read_bytes() == b'other',
                  'and neither file moved')
            # A finger: held on a disc set's folder, then onto the ROMs crumb
            page.dt.call('Emulation.setTouchEmulationEnabled', enabled=True, maxTouchPoints=1)
            (x0, y0) = at(row('Set'))
            (x1, y1) = at("document.querySelector('#crumbs button[data-path=\"roms\"]')")
            page.dt.call('Input.dispatchTouchEvent', type='touchStart', touchPoints=[{'x': x0, 'y': y0}])
            time.sleep(0.7)
            for k in range(1, 9):
                page.dt.call('Input.dispatchTouchEvent', type='touchMove',
                             touchPoints=[{'x': x0 + (x1 - x0) * k / 8, 'y': y0 + (y1 - y0) * k / 8}])
            page.dt.call('Input.dispatchTouchEvent', type='touchEnd', touchPoints=[])
            check(page.until(f"!{row('Set')}") and (card / 'Roms/Set/Set.m3u').is_file(),
                  "a finger held on a folder moved it whole onto a crumb above")
            page.until("!document.querySelector('#list li.sel')")
            # A finger held still on a row picks it, and the list is picking
            (x0, y0) = at(row('A Twice.nes'))
            page.dt.call('Input.dispatchTouchEvent', type='touchStart', touchPoints=[{'x': x0, 'y': y0}])
            time.sleep(0.7)
            page.dt.call('Input.dispatchTouchEvent', type='touchEnd', touchPoints=[])
            check(page.until(f"{row('A Twice.nes')}.classList.contains('sel') && !document.getElementById('selbar').hidden"),
                  'a finger held still on a row picks it, and the bar comes up')
            page.dt.call('Emulation.setTouchEmulationEnabled', enabled=False)
            server.stop()

            print('picking several, Move to folder and delete-many (1.0.6):')
            for n in ('A Panel.nes', 'A Del 1.nes', 'A Del 2.nes'):
                (nes / n).write_bytes(b'x')
            server = Server(card, web, 'usb', 'update=off')
            check(signed_in(page, server, 'roms/NES'), 'the page came up on NES')
            page.js("go('roms/NES', 'none')")
            page.until(f"cwd === 'roms/NES' && !!{row('A Panel.nes')}")
            page.js("scrollTo(0, 0)")

            def click(sel, mods=0):
                x, y = at(sel)
                page.dt.call('Input.dispatchMouseEvent', type='mouseMoved', x=x, y=y)
                page.dt.call('Input.dispatchMouseEvent', type='mousePressed', x=x, y=y, button='left', clickCount=1, modifiers=mods)
                page.dt.call('Input.dispatchMouseEvent', type='mouseReleased', x=x, y=y, button='left', clickCount=1, modifiers=mods)

            def key(k, mods=0):
                page.dt.call('Input.dispatchKeyEvent', type='keyDown', key=k, modifiers=mods,
                             windowsVirtualKeyCode={'Escape': 27, 'a': 65, 'Delete': 46}[k])
                page.dt.call('Input.dispatchKeyEvent', type='keyUp', key=k, modifiers=mods)

            picked = "document.querySelectorAll('#list li.sel').length"
            click(row('A Panel.nes'), 4)
            check(page.js(picked) == 1 and page.js("document.getElementById('list').classList.contains('picking')"),
                  'Cmd and a press pick a row (Ctrl is a right-click on a Mac); the list is picking')
            # Picking, a press on a folder picks it rather than opening it
            click(row('Hacks'))
            check(page.until(f"{picked} === 2") and page.js("cwd") == 'roms/NES',
                  'a press on a folder picks it and does not open it')
            check(page.js("document.getElementById('selcount').textContent") == '2 selected',
                  'the bar says how many: ' + str(page.js("document.getElementById('selcount').textContent")))
            click(row('Hacks'))
            check(page.until(f"{picked} === 1"), 'and a second press puts it down')
            click("document.getElementById('selall')")
            check(page.until(f"{picked} === entries.length") and page.js("document.getElementById('selall').hidden"),
                  'Select all picks every row and goes')
            key('Escape')
            check(page.until(f"{picked} === 0 && document.getElementById('selbar').hidden"), 'Escape puts them all down')
            key('a', 4)
            check(page.until(f"{picked} === entries.length"), 'Cmd+A picks every row')
            key('Escape')
            page.until(f"{picked} === 0")

            # Move to folder: the panel opens at ROMs, which is off; NES is
            # where they are, off too; Hacks is a place to go
            click(row('A Panel.nes'), 4)
            click("document.getElementById('selmove')")
            check(page.until("!document.getElementById('movedlg').hidden && mv && mv.at === 'roms'"),
                  'Move to folder opens at the folder above, ROMs')
            check(page.js("document.getElementById('moveyes').disabled") and
                  page.js("document.getElementById('movenote').textContent") == "Games go in a console's folder.",
                  'ROMs itself is off, and says why')
            folder = lambda n: f"[...document.querySelectorAll('#movelist button')].find((b) => b.querySelector('.name').textContent === {json.dumps(n)})"
            check(page.js(f"{folder('NES')}.querySelector('.tag').textContent") == 'current folder',
                  'the folder they are in is marked')
            click(folder('NES'))
            check(page.until("mv.at === 'roms/NES'") and page.js("document.getElementById('moveyes').disabled"),
                  'in the folder they are in, the button is off')
            click(folder('Hacks'))
            check(page.until("mv.at === 'roms/NES/Hacks'") and
                  page.js("document.getElementById('moveyes').textContent") == 'Move to Hacks' and
                  not page.js("document.getElementById('moveyes').disabled"), 'a folder in it: Move to Hacks')
            click("document.querySelector('#movecrumbs button:last-of-type')")
            check(page.until("mv.at === 'roms/NES'"), 'a crumb goes back up')
            # New folder, made where the panel is and opened
            click("document.getElementById('movenew')")
            page.until("!document.getElementById('dialog').hidden")
            page.js("document.getElementById('dlgfield').value = 'Made'; document.getElementById('dlgform').requestSubmit()")
            check(page.until("mv && mv.at === 'roms/NES/Made' && !document.getElementById('movedlg').hidden") and
                  (nes / 'Made').is_dir(), 'New folder makes it there and opens it')
            click("document.getElementById('moveyes')")
            check(page.until(f"!{row('A Panel.nes')}") and (nes / 'Made/A Panel.nes').is_file() and
                  page.js("document.getElementById('movedlg').hidden"), 'Move to Made moved it there')
            check(page.until("document.getElementById('selbar').hidden"), 'and nothing is picked after')
            toast = "document.getElementById('toast')"
            check(page.until(f"{toast}.classList.contains('show') && {toast}.firstChild.textContent === 'Moved A Panel.nes to Made'")
                  and page.js(f"{toast}.querySelector('.act').textContent") == 'Undo',
                  'a notice says what moved where, with Undo: ' + str(page.js(f"{toast}.textContent")))
            click(f"{toast}.querySelector('.act')")
            check(page.until(f"!!{row('A Panel.nes')}") and (nes / 'A Panel.nes').is_file() and
                  not (nes / 'Made/A Panel.nes').exists() and
                  page.until(f"{toast}.textContent === 'Moved back.'"), 'Undo moved it back where it was')
            # Several: the notice counts them, and Undo takes each back
            click(row('A Del 1.nes'), 4)
            click(row('A Del 2.nes'), 4)
            click("document.getElementById('selmove')")
            page.until("mv && mv.at === 'roms'")
            click(folder('NES'))
            page.until("mv.at === 'roms/NES'")
            click(folder('Hacks'))
            page.until("mv.at === 'roms/NES/Hacks'")
            click("document.getElementById('moveyes')")
            check(page.until(f"{toast}.firstChild.textContent === 'Moved 2 items to Hacks'") and
                  (nes / 'Hacks/A Del 1.nes').is_file() and (nes / 'Hacks/A Del 2.nes').is_file(),
                  'two moved together: ' + str(page.js(f"{toast}.firstChild.textContent")))
            click(f"{toast}.querySelector('.act')")
            check(page.until(f"!!{row('A Del 1.nes')} && !!{row('A Del 2.nes')}") and
                  (nes / 'A Del 1.nes').is_file() and (nes / 'A Del 2.nes').is_file(), 'and Undo took both back')

            # Delete several: one question, then both go
            click(row('A Del 1.nes'), 4)
            click(row('A Del 2.nes'), 4)
            click("document.getElementById('seldel')")
            check(page.until("!document.getElementById('dialog').hidden") and
                  page.js("document.getElementById('dlghead').textContent") == 'Delete 2 items?',
                  'Delete asks once for the two: ' + str(page.js("document.getElementById('dlghead').textContent")))
            click("document.getElementById('dlgyes')")
            check(page.until(f"!{row('A Del 1.nes')} && !{row('A Del 2.nes')}") and
                  not (nes / 'A Del 1.nes').exists() and not (nes / 'A Del 2.nes').exists(), 'and both are gone')
            server.stop()

            print("the page in the handheld's language:")
            server = Server(card, web, 'usb', 'update=off', 'space=88.4/119', 'lang=fr')
            check(signed_in(page, server) and
                  page.until("document.getElementById('roomline').textContent !== ''"),
                  'the page came up past the PIN, in French')
            got = page.js("""(() => ({ lang: document.documentElement.lang, title: document.title,
                name: document.querySelector('.appname').textContent,
                gate: document.querySelector('#gate .pagename').textContent,
                room: document.getElementById('roomline').textContent,
                keys: [...document.querySelectorAll('*')].flatMap((e) => [e.getAttribute('aria-label') || '',
                    e.childNodes.length === 1 && e.firstChild.nodeType === 3 ? e.textContent : ''])
                    .filter((w) => /^[a-z_]+(\\.[a-z_]+)+$/.test(w)) }))()""")
            check(got['lang'] == 'fr' and got['title'] == 'Transfert de fichiers USB' and
                  got['name'] == got['title'] and got['gate'] == got['title'],
                  f"the page's language and the cable's name: {got['lang']}, {got['title']}, {got['name']}, {got['gate']}")
            check(got['room'] == '88,3 Go libres sur 119,0 Go',
                  f"sizes the language's way: {got['room']}")
            check(not got['keys'], f"every word found its string: {got['keys']}")
            # The same server and the same browser, its cache warm with the
            # page's tagged files, opened over the other link and back: the
            # name is the link's each time, never one the browser kept
            names = "[document.title, document.querySelector('.appname').textContent, " \
                    "document.querySelector('#gate .pagename').textContent]"
            for link, name in (('wifi', 'Transfert de fichiers Wi-Fi'), ('usb', 'Transfert de fichiers USB')):
                server.say(link)
                fresh(page, f'{server.origin}/#roms')
                check(page.until(f"document.readyState === 'complete' && {names}.every((n) => n === {json.dumps(name)})"),
                      f'opened again over {link}, the page is {name}: {page.js(names)}')
            server.stop()

            print("over Wi-Fi, Wi-Fi's name:")
            server = Server(card, web, 'update=off')
            check(signed_in(page, server) and page.js(f"{names}.every((n) => n === 'Wi-Fi File Transfer')"),
                  f"the page is Wi-Fi File Transfer: {page.js(names)}")
            server.stop()

            print('a drop in the middle of an upload: the file waits, then goes on:')
            server = Server(card, web, 'update=off')
            nes = card / 'Roms/NES'
            check(signed_in(page, server, 'roms/NES'), 'the page came up past the PIN')
            offline = lambda on, up=-1: page.dt.call('Network.emulateNetworkConditions', offline=on, latency=0,
                                                     downloadThroughput=-1, uploadThroughput=up)
            head = "document.getElementById('queuehead').textContent"
            left = "document.getElementById('queueleft').textContent"
            row = "(document.querySelector('#queuelist .p') || {}).textContent"
            shown = lambda id: f"!document.getElementById('{id}').hidden"
            pattern = bytes(i * 13 % 251 for i in range(3000000))
            page.js("""(() => {
                Object.assign(RESUME, { min: 0, ask: 300, wait: 1000, stall: 3000, check: 1500 });
                window.__puts = [];
                const open = XMLHttpRequest.prototype.open;
                XMLHttpRequest.prototype.open = function (m, url) { window.__puts.push(url); return open.apply(this, arguments); };
                const d = new Uint8Array(3000000).map((_, i) => i * 13 % 251);
                window.__drop = (name) => new File([d], name, { lastModified: 1731500000000 });
            })()""")

            def cut(name):
                """The file going up slowly, and a third of the way in the
                handheld off the network, its connections gone with it
                (portal-serve's away and cut: Chrome's offline lets an upload
                on its way finish)"""
                offline(False, 262144)
                page.js(f"enqueue([__drop({json.dumps(name)})], 'roms/NES')")
                ok = page.until("queue.length > 0 && queue[0].loaded > 900000", 20)
                server.say('away')
                server.say('cut')
                offline(False)
                return ok and page.until(f"{head} === 'Waiting for the handheld'", 10)

            check(cut('Drop.bin'), 'the network gone, the file waits for the handheld: ' + str(page.js(head)))
            check(page.js(f"{left}.startsWith('Connection lost. ') && {left}.endsWith(' is kept on the handheld, "
                          f"and the rest goes when it answers.')") and
                  page.js(f"/^paused at \\d+%$/.test({row})") and page.js(shown('queuestop')),
                  'it says what is kept, the row is paused, and Stop sending is there: ' +
                  str(page.js(f"[{left}, {row}]")))
            check(page.until(shown('queuecheck'), 15) and
                  page.js("document.getElementById('queuecheck').textContent") ==
                  f"Still no answer. Check that File Transfer is open on the handheld at {server.origin[7:]}.",
                  'after a while, the address to check: ' + str(page.js("document.getElementById('queuecheck').textContent")))
            check((nes / 'Drop.bin.part').is_file() and not (nes / 'Drop.bin').exists(),
                  'the handheld kept a part and nothing under the name')
            server.say('back')
            check(page.until("queue[0].state === 'done'", 30), 'the network back, it went on: ' +
                  str(page.js("queue.map((j) => j.state + ' ' + (j.why || ''))")))
            puts = page.js("window.__puts")
            check(len(puts) >= 2 and '&from=0' in puts[0] and '&from=0' not in puts[-1],
                  f'the second try went on from where the first stopped: {puts}')
            check((nes / 'Drop.bin').read_bytes() == pattern and not (nes / 'Drop.bin.part').exists(),
                  'on the card to the byte, and no part left')
            page.until("queue.length === 0", 5)

            print('  a drop the browser does not notice: a file that stops moving waits too:')
            page.js("window.__puts.length = 0")
            offline(False, 262144)
            page.js("enqueue([__drop('Still.bin')], 'roms/NES')")
            check(page.until("queue.length > 0 && queue[0].loaded > 600000", 20), 'it went up a while')
            server.say('away')
            # The computer's own buffers take what they can first, a few
            # seconds of it at this speed
            check(page.until(f"{head} === 'Waiting for the handheld'", 40), 'stopped moving, it waits: ' + str(page.js(head)))
            offline(False)
            server.say('back')
            check(page.until("queue[0].state === 'done'", 30), 'the handheld back, it went on')
            check((nes / 'Still.bin').read_bytes() == pattern and not (nes / 'Still.bin.part').exists(),
                  'on the card to the byte, and no part left')
            page.until("queue.length === 0", 5)

            print('  Stop sending:')
            page.js("RESUME.ask = 60000")
            check(cut('Stopped.bin'), 'waiting for the handheld again')
            check((nes / 'Stopped.bin.part').is_file(), 'a part kept')
            # Stopped while the handheld is still away, so nothing can go on
            # first; its DELETE waits for it to come back
            page.js("document.getElementById('queuestop').click()")
            server.say('back')
            check(page.until(f"{head} === '0 sent, 1 not sent'", 5) and page.js(f"{row} === 'not sent'"),
                  'it stopped, and the file stayed here: ' + str(page.js(f"[{head}, {row}]")))
            check(page.until("document.getElementById('queueacts').hidden", 2), 'no button is left')
            # Its DELETE may wait for a connection a while: the handheld's
            # queue of them filled with the page's asks while it was away
            for _ in range(250):
                if not (nes / 'Stopped.bin.part').exists():
                    break
                time.sleep(0.1)
            check(not (nes / 'Stopped.bin.part').exists() and not (nes / 'Stopped.bin').exists(),
                  'the handheld let go of its part')
            page.js("fresh()")

            print('  ten minutes with no answer, and Try again:')
            page.js("Object.assign(RESUME, { ask: 300, giveUp: 2000 })")
            check(cut('Late.bin'), 'waiting for the handheld once more')
            # A browser may send the broken PUT again by itself, into its own
            # buffers, before it stalls: the wait counts from then
            check(page.until(f"{head} === '0 sent, 1 failed'", 40) and
                  page.js(f"/^The handheld did not answer for 10 minutes. Try again to go on from \\d+%.$/.test({left})") and
                  page.js(f"{row} === 'connection lost'") and page.js(shown('queueretry')) and
                  not page.js(shown('queuestop')),
                  'it gave up, said so, and offers Try again: ' + str(page.js(f"[{head}, {left}, {row}]")))
            server.say('back')
            page.js("window.__puts.length = 0; document.getElementById('queueretry').click()")
            check(page.until("queue[0].state === 'done'", 30), 'Try again sent it: ' +
                  str(page.js("queue.map((j) => j.state + ' ' + (j.why || ''))")))
            puts = page.js("window.__puts")
            check(puts and all('&from=0' not in u for u in puts), f'from the part the handheld kept: {puts}')
            check((nes / 'Late.bin').read_bytes() == pattern, 'on the card to the byte')
            page.until("queue.length === 0", 5)

            print('  loaded again, the same file picked again:')
            check(cut('Again.bin'), 'waiting for the handheld before the page is left')
            # Left while the handheld is away, so this page cannot go on with
            # it, and opened again once it is back
            page.js("sending = false")
            page.dt.call('Page.navigate', url='about:blank')
            page.until("location.href === 'about:blank'")
            server.say('back')
            time.sleep(0.5)
            check((nes / 'Again.bin.part').is_file(), 'the part is kept')
            fresh(page, f'{server.origin}/#roms/NES')
            check(page.until("document.readyState === 'complete' && typeof enqueue === 'function' && "
                             "!document.getElementById('app').hidden", 10), 'the page came up again, still signed in')
            page.js("""(() => {
                RESUME.min = 0;
                window.__puts = [];
                const open = XMLHttpRequest.prototype.open;
                XMLHttpRequest.prototype.open = function (m, url) { window.__puts.push(url); return open.apply(this, arguments); };
                const d = new Uint8Array(3000000).map((_, i) => i * 13 % 251);
                enqueue([new File([d], 'Again.bin', { lastModified: 1731500000000 })], 'roms/NES');
            })()""")
            check(page.until("queue.length > 0 && queue[0].state === 'done'", 30), 'the file picked again went')
            puts = page.js("window.__puts")
            check(puts and all('&from=0' not in u for u in puts), f'it went on from the kept part: {puts}')
            check((nes / 'Again.bin').read_bytes() == pattern, 'on the card to the byte')
            offline(False)
            server.stop()
        finally:
            page.close()
    if failures:
        print(f'\n{failures} check(s) failed')
        return 1
    print('\nok: the page wears its art, hides dot folders, counts the room, says the time, '
          'follows the handheld\'s stop, updates over the cable, moves files by dragging, '
          'speaks its language, is named after its link and goes on after a drop')
    return 0


if __name__ == '__main__':
    sys.exit(main())
