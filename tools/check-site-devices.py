#!/usr/bin/env python3
"""Check that zyperos.com takes any number of handhelds from site/devices.json.

python3 tools/check-site-devices.py [--shots DIR] [--chrome PATH]

Builds the site twice with made-up handhelds added to the real list (one, so
three in all, then three, so five), with no other change to the page, and
opens each build in headless Chrome at a computer's width (1440) and a
phone's (375). It fails on a script or console error, a picture that does not
load, any sideways scroll, a pick or a name that does not fit, the hero's
words under its handheld, or a choice the page does not follow: each pick
must put its handheld on the stage, its facts and its fact note, its
captures in the tour, and its button in the install step. --shots keeps a
picture of each hero. The made-up handhelds reuse the Brick's model and
captures: they test the page, not a device.
"""
import argparse
import base64
import copy
import functools
import http.server
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("readme_shots", ROOT / "tools/readme-shots.py")
shots_tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(shots_tool)
DevTools = shots_tool.DevTools

FAKE_NAMES = [("testone", "Test Handheld One", "Test One", [80, 128]),
              ("testtwo", "Test Two", "Test Two", [58, 92]),
              ("testthree", "Test Handheld Three XL", "Three", [96, 150])]

# Errors as the page meets them: scripts, the console, pictures and files
WATCH = """window.__errs = [];
addEventListener('error', e => __errs.push(e.message ? 'script: ' + e.message
  : 'load: ' + (e.target && (e.target.currentSrc || e.target.src || e.target.href))), true);
addEventListener('unhandledrejection', e => __errs.push('promise: ' + e.reason));
const __ce = console.error; console.error = (...a) => { __errs.push('console: ' + a.join(' ')); __ce.apply(console, a); };"""

# The page's own checks, in the page; a list of problems
CHECK = r"""(async () => {
  const P = [], wait = ms => new Promise(r => setTimeout(r, ms));
  const shown = e => e && getComputedStyle(e).display !== 'none' && e.getClientRects().length > 0;
  if (scrollY > 0) P.push(`the page opened scrolled to ${scrollY}, not its top`);
  const W = innerWidth, data = JSON.parse(document.querySelector('#device-data').textContent).devices;
  if (document.documentElement.scrollWidth > W) P.push(`sideways scroll: ${document.documentElement.scrollWidth} > ${W}`);
  const picks = [...document.querySelectorAll('.pick')];
  if (picks.length !== data.length) P.push(`${picks.length} picks for ${data.length} handhelds`);
  const row = document.querySelector('.picks');
  if (row.getBoundingClientRect().right > W + 1 || row.getBoundingClientRect().left < -1) P.push('the row of picks leaves the page');
  for (const name of document.querySelectorAll('.pick-name'))
    if (name.scrollWidth > name.clientWidth + 1) P.push(`a pick's name does not fit: ${name.textContent}`);
  const copyBox = document.querySelector('.hero-copy').getBoundingClientRect();
  const product = document.querySelector('.hero-product').getBoundingClientRect();
  const overlap = Math.min(copyBox.right, product.right) - Math.max(copyBox.left, product.left) > 1
               && Math.min(copyBox.bottom, product.bottom) - Math.max(copyBox.top, product.top) > 1;
  if (overlap) P.push("the hero's words and its handheld overlap");
  for (const d of data) {
    const input = document.getElementById(d.id);
    const y = scrollY;
    input.closest('.pick').click();
    await wait(700);
    if (scrollY !== y) P.push(`${d.id}: choosing it scrolled the page`);
    const pr = input.closest('.pick').getBoundingClientRect(), rr = row.getBoundingClientRect();
    if (pr.left < rr.left - 1 || pr.right > rr.right + 1) P.push(`${d.id}: its pick, chosen, is not in view in the row`);
    if (!input.checked) { P.push(`${d.id}: its pick does not choose it`); continue; }
    const stage = [...document.querySelectorAll('.stage .device')].filter(shown);
    if (stage.length !== 1 || stage[0].dataset.model !== d.id) P.push(`${d.id}: the stage shows ${stage.map(s => s.dataset.model).join(',') || 'nothing'}`);
    const facts = [...document.querySelectorAll('.facts > div')].filter(shown);
    if (facts.length !== 4) P.push(`${d.id}: ${facts.length} facts shown, not 4`);
    const notes = [...document.querySelectorAll('.fact-note')].filter(shown);
    if (notes.length !== 1) P.push(`${d.id}: ${notes.length} fact notes shown`);
    const pressed = [...document.querySelectorAll('[data-pick][aria-pressed="true"]')].map(b => b.dataset.pick);
    if (pressed.join() !== d.id) P.push(`${d.id}: the install step's button is ${pressed.join() || 'none'}`);
    const tour = document.querySelector('#tour-image').getAttribute('src');
    if (d.captures && !tour.startsWith(`img/${d.captures}/`)) P.push(`${d.id}: the tour shows ${tour}`);
    if (document.documentElement.scrollWidth > W) P.push(`${d.id}: sideways scroll`);
  }
  document.getElementById(data[0].id).closest('.pick').click();
  await wait(1500);
  return P.concat(window.__errs);
})()"""


def fakes(count):
    real = json.loads((ROOT / "site/devices.json").read_text())
    brick = next(d for d in real["devices"] if d["id"] == "brick")
    for ident, name, short, size in FAKE_NAMES[:count]:
        d = copy.deepcopy(brick)
        d.update(id=ident, name=name, short=short, size_mm=size, faq=[], lines={}, install="",
                 hidden_systems=["PSP"], hidden_note="one comes later.",
                 missing={"ports": None, "player2": None, "shaders-old": "two of six, for now"},
                 fact_note="*Made up, for this check.")
        d["facts"] = [{"value": str(40 + len(ident)), "label": "Systems, made up", "note": True},
                      {"value": "Test", "label": "A made-up fact"}, {"value": "1", "label": "Another"}]
        real["devices"].append(d)
    return real


def serve(folder):
    class Quiet(http.server.SimpleHTTPRequestHandler):
        def log_message(self, *args):
            pass
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Quiet, directory=str(folder)))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def chrome_session(chrome, profile):
    browser = subprocess.Popen([chrome, "--headless=new", "--remote-debugging-port=0", f"--user-data-dir={profile}",
                                "--no-first-run", "--no-default-browser-check", "--hide-scrollbars",
                                "--disable-background-networking", "--disable-component-update", "--disable-sync",
                                "--no-pings", "about:blank"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(100):
        port_file = profile / "DevToolsActivePort"
        if port_file.is_file() and port_file.read_text().strip():
            break
        time.sleep(0.1)
    debug = (profile / "DevToolsActivePort").read_text().split()[0]
    target = next(t for t in json.load(urllib.request.urlopen(f"http://127.0.0.1:{debug}/json/list")) if t["type"] == "page")
    return browser, DevTools(target["webSocketDebuggerUrl"])


def run(chrome, extra, shots):
    problems = []
    with tempfile.TemporaryDirectory(prefix="zyperos-devices-") as temporary:
        base = Path(temporary)
        (base / "devices.json").write_text(json.dumps(fakes(extra)))
        out = base / "site"
        build = subprocess.run(["sh", str(ROOT / "mk/site.sh"), str(out)], cwd=ROOT, text=True,
                               env={**os.environ, "SITE_DEVICES": str(base / "devices.json")},
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if build.returncode:
            return [f"{2 + extra} handhelds: the site does not build:\n{build.stdout[-2000:]}"]
        server = serve(out)
        browser, dt = chrome_session(chrome, base / "chrome")
        try:
            dt.call("Page.enable")
            dt.call("Page.addScriptToEvaluateOnNewDocument", source=WATCH)
            dt.call("Emulation.setEmulatedMedia", features=[{"name": "prefers-color-scheme", "value": "light"},
                                                           {"name": "prefers-reduced-motion", "value": "reduce"}])
            for width, height, phone in ((1440, 900, False), (375, 812, True)):
                dt.call("Emulation.setDeviceMetricsOverride", width=width, height=height, deviceScaleFactor=1, mobile=phone)
                dt.call("Page.navigate", url=f"http://127.0.0.1:{server.server_port}/?check={width}")
                for _ in range(200):
                    state = dt.call("Runtime.evaluate", returnByValue=True, expression="document.readyState")
                    if state.get("result", {}).get("value") == "complete":
                        break
                    time.sleep(0.1)
                time.sleep(2.5)
                if shots:
                    clip = dt.call("Runtime.evaluate", returnByValue=True, expression=
                                   "(() => { const r = document.querySelector('#why').getBoundingClientRect(); return r.bottom + scrollY; })()")["result"]["value"]
                    dt.call("Emulation.setDeviceMetricsOverride", width=width, height=int(clip) + 10, deviceScaleFactor=1, mobile=phone)
                    time.sleep(0.8)
                    shot = dt.call("Page.captureScreenshot", format="png")["data"]
                    Path(shots, f"hero-{2 + extra}-{width}.png").write_bytes(base64.b64decode(shot))
                    dt.call("Emulation.setDeviceMetricsOverride", width=width, height=height, deviceScaleFactor=1, mobile=phone)
                    time.sleep(0.3)
                found = dt.call("Runtime.evaluate", returnByValue=True, awaitPromise=True, expression=CHECK)
                if "exceptionDetails" in found:
                    problems.append(f"{2 + extra} handhelds at {width}: the check itself failed: {found['exceptionDetails']}")
                    continue
                problems += [f"{2 + extra} handhelds at {width}: {p}" for p in found["result"]["value"]]
        finally:
            browser.terminate()
            browser.wait(timeout=10)
            server.shutdown()
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--shots", type=Path, help="keep a picture of each hero here")
    ap.add_argument("--chrome", default=shots_tool.CHROME)
    a = ap.parse_args()
    if not Path(a.chrome).is_file():
        sys.exit(f"check-site-devices: no Chrome at {a.chrome} (--chrome)")
    if a.shots:
        a.shots.mkdir(parents=True, exist_ok=True)
    problems = run(a.chrome, 1, a.shots) + run(a.chrome, 3, a.shots)
    for p in problems:
        print("check-site-devices:", p)
    if problems:
        sys.exit(1)
    print("check-site-devices: 3 and 5 handhelds, at 1440 and 375, every pick followed, no error")


if __name__ == "__main__":
    main()
