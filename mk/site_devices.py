#!/usr/bin/env python3
"""Build the landing page's device parts from site/devices.json.

python3 mk/site_devices.py --out OUT [--devices FILE]

mk/site.sh runs it on OUT/index.html after copying the site, before it
versions the page's addresses. Every handheld in the list gets:

- its model, poster and thumbnail copied to OUT/models, and its captures
  (res/readme/<captures>/, by the first handheld's names) to OUT/img/<captures>/
- a place in the hero: its own large stage, a pick in the row of handhelds
  (the radio buttons the whole page follows), drawn to scale
- its facts and their note, its install note, its FAQ entries
- a mark wherever the page offers something it doesn't have yet (an
  element's data-needs="<key>", the key in its "missing"), on the systems it
  hides (data-system), and its own words for a data-line element ("lines",
  or {short} and {name} filled in)
- {{device_names}}, {{device_names_or}} and {{device_list}} in the page, the
  names joined ("A, B and C", "A, B or C", "A, B, C")

Then the CSS that shows one handheld's lines at a time (:has(), and
html[data-device] for the page's script) and the list the page's script
reads (#device-data). Python's standard library only, as on Cloudflare Pages.
"""
import argparse
import hashlib
import html
import json
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# The page's tour and the 3D screen: the first handheld's capture names
TOUR = ["systems", "shelf-n64", "theme", "mp4", "menu"]
TITLE_MAX, DESCRIPTION_MAX = 65, 160


def esc(text):
    return html.escape(text, quote=True)


def fail(message):
    sys.exit("site_devices: " + message)


def joined(names, word="and"):
    if len(names) < 3:
        return f" {word} ".join(names)
    return ", ".join(names[:-1]) + f" {word} " + names[-1]


def version(out, rel):
    return rel + "?v=" + hashlib.sha256((out / rel).read_bytes()).hexdigest()[:12]


def load(path):
    devices = json.loads(path.read_text(encoding="utf-8"))["devices"]
    if not devices:
        fail("no handhelds in " + str(path))
    seen = set()
    for d in devices:
        for key in ("id", "name", "short", "size_mm", "model", "facts", "fact_note"):
            if key not in d:
                fail(f"{d.get('id', '?')}: no {key}")
        if not re.fullmatch(r"[a-z][a-z0-9]*", d["id"]) or d["id"] in seen:
            fail(f"{d['id']}: an id is lower-case letters and digits, once")
        seen.add(d["id"])
        for key in ("glb", "poster", "thumb", "poster_px", "frame_mm", "label"):
            if key not in d["model"]:
                fail(f"{d['id']}: its model has no {key}")
        if len(d["facts"]) != 3:
            fail(f"{d['id']}: three facts, beside the page's own fourth")
        d.setdefault("captures", "")
        for key, empty in (("swap", {}), ("alts", {}), ("missing", {}), ("lines", {}),
                           ("hidden_systems", []), ("faq", [])):
            d.setdefault(key, empty)
    return devices


def copy_assets(out, devices):
    for d in devices:
        for key in ("glb", "poster", "thumb"):
            rel = d["model"][key]
            src = ROOT / "site" / rel
            if not src.is_file():
                fail(f"{d['id']}: no site/{rel}")
            (out / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(src, out / rel)
        d["shots"], d["dark"] = [], []
        if d["captures"]:
            src = ROOT / "res/readme" / d["captures"]
            if not src.is_dir():
                fail(f"{d['id']}: no res/readme/{d['captures']}/")
            for sub, names in (("", d["shots"]), ("dark/", d["dark"])):
                dest = out / "img" / d["captures"] / sub
                dest.mkdir(parents=True, exist_ok=True)
                for png in sorted((src / sub).glob("*.png")) if (src / sub).is_dir() else []:
                    shutil.copyfile(png, dest / png.name)
                    names.append(png.stem)


def frame(d):
    """The poster's frame and the body in it, in millimetres"""
    pw, ph = d["model"]["poster_px"]
    fh = d["model"]["frame_mm"]
    return fh * pw / ph, fh, d["size_mm"][0], d["size_mm"][1]


def hero(out, devices):
    stage, picks = [], []
    for n, d in enumerate(devices):
        fw, fh, bw, bh = frame(d)
        m = d["model"]
        pw, ph = m["poster_px"]
        floor = (fh - bh) / 2 / fh * 100
        style = f"--ratio:{pw}/{ph};--floor:{floor:.2f}%;--body:{bw / fw * 100:.1f}%"
        first = n == 0
        stage.append(
            f'<span class="device" data-model="{d["id"]}" data-for="{d["id"]}" style="{style}">'
            f'<img class="device-poster" src="{version(out, m["poster"])}" width="{pw}" height="{ph}"'
            + (' fetchpriority="high"' if first else ' loading="lazy" decoding="async"')
            + f' alt="{esc(m["label"])} running ZyperOS.">'
            f'<canvas tabindex="0" role="img" aria-label="{esc(m["label"])}. Drag or use the left and right arrow keys to rotate it." hidden></canvas></span>')
        tw, th = round(pw / 4), round(ph / 4)
        pick = (f"--frame-w:{fw:.2f};--frame-h:{fh:.2f};--body-w:{bw};--body-h:{bh}")
        picks.append(
            f'<label class="pick" style="{pick}">'
            f'<input class="visually-hidden" type="radio" name="device" id="{d["id"]}" value="{d["id"]}"' + (" checked" if first else "") + ">"
            f'<img src="{version(out, m["thumb"])}" width="{tw}" height="{th}" alt="" draggable="false">'
            f'<span class="pick-name">{esc(d["name"])}</span></label>')
    # The choice is kept for the next visit, and #<id> opens the page on it
    restore = ("<script>try { var e = document.getElementById(location.hash.slice(1)); "
               "if (!e || e.name !== 'device') { var s = localStorage.getItem('zyperos-device'); e = s && document.getElementById(s); } "
               "if (e && e.name === 'device') e.checked = true; } catch (x) {}</script>")
    return ('<div class="stage">' + "".join(stage) + "</div>\n"
            '      <fieldset class="handhelds"><legend class="visually-hidden">Your handheld. The page shows its screens.</legend>'
            '<div class="picks">' + "".join(picks) + "</div></fieldset>\n      " + restore)


def facts(devices):
    parts = []
    for i in range(3):
        for d in devices:
            f = d["facts"][i]
            unit = f'<span>{esc(f["unit"])}</span>' if f.get("unit") else ""
            note = "<sup>*</sup>" if f.get("note") else ""
            parts.append(f'<div data-for="{d["id"]}"><strong>{esc(f["value"])}{unit}</strong><p>{esc(f["label"])}{note}</p></div>')
    return "\n    ".join(parts)


def fact_notes(devices):
    return "\n    ".join(f'<p class="fact-note" data-for="{d["id"]}">{esc(d["fact_note"])}</p>' for d in devices)


def install(devices):
    buttons = "".join(f'<button type="button" data-pick="{d["id"]}" aria-pressed="{str(n == 0).lower()}">{esc(d["name"])}</button>'
                      for n, d in enumerate(devices))
    notes = "".join(f'<p class="install-note" data-for="{d["id"]}">{esc(d["install"])}</p>' for d in devices if d.get("install"))
    return f'<div class="device-pick" role="group" aria-label="Your handheld" hidden>{buttons}</div>{notes}'


def after_install(devices):
    return "".join(f'<span data-for="{d["id"]}"> {esc(d["after_install"])}</span>' for d in devices if d.get("after_install"))


def faq(devices):
    out = []
    for d in devices:
        for q in d["faq"]:
            body = "".join(f"<p>{esc(p)}</p>" for p in q["a"])
            link = f'<a href="#{d["id"]}">{esc(q["link"])}</a>' if q.get("link") else ""
            ident = f' id="{q["id"]}"' if q.get("id") else ""
            out.append(f'<details{ident}><summary>{esc(q["q"])} <span aria-hidden="true">+</span></summary>{body}{link}</details>')
    return "\n      ".join(out)


def element_end(page, start, tag):
    """The end of the element whose start tag begins at start: tags of the
    same name counted, as the page writes them (no self-closing ones)"""
    depth, pos = 0, start
    pattern = re.compile(rf"<(/?){tag}\b[^>]*>")
    for m in pattern.finditer(page, start):
        depth += -1 if m.group(1) else 1
        if depth == 0:
            return m.start(), m.end()
    fail(f"an unclosed <{tag}> at {start}")


def marks(page, devices):
    total = len(re.findall(r'<li data-system="', page))
    def mark(key):
        if key == "systems":
            # Its own: how many of the page's systems each handheld shows
            return "".join(f' <span class="not-yet" data-devices="{d["id"]}">{esc(d["short"])}: {total - len(d["hidden_systems"])} for now</span>'
                           for d in devices if d["hidden_systems"])
        lacking = [d for d in devices if key in d["missing"]]
        plain = [d for d in lacking if d["missing"][key] is None]
        out = []
        if plain:
            out.append((" ".join(d["id"] for d in plain),
                        "Not on the " + joined([d["short"] for d in plain], "or the") + " yet"))
        out += [(d["id"], f'{d["short"]}: {d["missing"][key]}') for d in lacking if d["missing"][key] is not None]
        return "".join(f' <span class="not-yet" data-devices="{ids}">{esc(words)}</span>' for ids, words in out)
    known = set()
    for m in reversed(list(re.finditer(r'<(\w+)\b[^>]*\bdata-needs="([\w-]+)"[^>]*>', page))):
        tag, key = m.group(1), m.group(2)
        known.add(key)
        close, _ = element_end(page, m.start(), tag)
        page = page[:close] + mark(key) + page[close:]
    for d in devices:
        for key in d["missing"]:
            if key not in known:
                fail(f"{d['id']}: missing names {key}, which no data-needs on the page has")
    return page


def systems(page, devices):
    total = len(re.findall(r'<li data-system="', page))
    def one(m):
        key, name = m.group(1), m.group(2)
        hiding = [d for d in devices if key in d["hidden_systems"]]
        if not hiding:
            return m.group(0)
        words = ", not on the " + joined([d["short"] for d in hiding], "or the") + " yet"
        return (f'<li data-system="{key}" class="later" data-devices="{" ".join(d["id"] for d in hiding)}">'
                f'{name}<span class="visually-hidden">{esc(words)}</span></li>')
    page = re.sub(r'<li data-system="(\w+)">(.*?)</li>', one, page)
    keys = set(re.findall(r'<li data-system="(\w+)"', page))
    for d in devices:
        for key in d["hidden_systems"]:
            if key not in keys:
                fail(f"{d['id']}: hides {key}, which no data-system on the page has")
    notes = "".join(f'<p class="small" data-for="{d["id"]}">On the {esc(d["short"])}, {total - len(d["hidden_systems"])} for now: {esc(d["hidden_note"])}</p>'
                    for d in devices if d["hidden_systems"])
    hiding = [d["short"] for d in devices if d["hidden_systems"]]
    legend = ('<span class="later-key" aria-hidden="true"></span>Not on the ' + esc(joined(hiding, "or the")) + " yet. ") if hiding else ""
    return page.replace("<!--devices:systems-->", notes).replace("<!--devices:systems-legend-->", legend)


def lines(page, devices):
    known = set()
    for m in reversed(list(re.finditer(r'<(\w+)\b([^>]*?)\s*data-line="([\w-]+)"([^>]*)>', page))):
        tag, key = m.group(1), m.group(3)
        known.add(key)
        close, end = element_end(page, m.start(), tag)
        inner = page[m.end():close]
        attrs = (m.group(2) + m.group(4)).strip()
        groups = {}
        for d in devices:
            text = d["lines"].get(key)
            text = esc(text) if text is not None else inner.replace("{short}", esc(d["short"])).replace("{name}", esc(d["name"]))
            groups.setdefault(text, []).append(d["id"])
        if len(groups) == 1:
            whole = f"<{tag}{' ' + attrs if attrs else ''}>{next(iter(groups))}</{tag}>"
        else:
            whole = f"<{tag}{' ' + attrs if attrs else ''}>" + "".join(
                f'<span data-for="{" ".join(ids)}">{text}</span>' for text, ids in groups.items()) + f"</{tag}>"
        page = page[:m.start()] + whole + page[end:]
    for d in devices:
        for key in d["lines"]:
            if key not in known:
                fail(f"{d['id']}: lines names {key}, which no data-line on the page has")
    return page


def rules(devices):
    css = []
    for d in devices:
        i = d["id"]
        chosen = (f':root:has(#{i}:checked)', f'html[data-device="{i}"]')
        css.append(", ".join(f'{c} [data-for]:not([data-for~="{i}"])' for c in chosen) + " { display: none !important; }")
        css.append(", ".join(f'{c} .not-yet[data-devices~="{i}"], {c} .later[data-devices~="{i}"]' for c in chosen)
                   + " { opacity: 1; border-color: color-mix(in srgb, currentColor 60%, transparent); }")
    return "<style>\n" + "\n".join(css) + "\n</style>"


def data(out, devices):
    rows = [{"id": d["id"], "name": d["name"], "short": d["short"], "captures": d["captures"],
             "shots": d["shots"], "dark": d["dark"], "swap": d["swap"], "alts": d["alts"],
             "glb": version(out, d["model"]["glb"]), "frame_mm": d["model"]["frame_mm"],
             "studio": d["model"].get("studio", {})} for d in devices]
    text = json.dumps({"tour": TOUR, "devices": rows}, separators=(",", ":")).replace("</", "<\\/")
    return f'<script type="application/json" id="device-data">{text}</script>'


def build(out, devices_file):
    devices = load(devices_file)
    copy_assets(out, devices)
    page_path = out / "index.html"
    page = page_path.read_text(encoding="utf-8")
    ids = set(re.findall(r'\bid="([^"]+)"', page))
    for d in devices:
        if d["id"] in ids:
            fail(f"{d['id']}: the page already has an element with that id")
    names = [d["name"] for d in devices]
    for token, words in (("{{device_names}}", joined(names)), ("{{device_names_or}}", joined(names, "or")),
                         ("{{device_list}}", ", ".join(names))):
        page = page.replace(token, esc(words))
    # A title or description too long for every name names none of them
    title = re.search(r"<title>(.*?)</title>", page).group(1)
    if len(html.unescape(title)) > TITLE_MAX:
        page = page.replace(f"<title>{title}</title>", "<title>" + title.replace(esc(joined(names)), "Retro Handhelds") + "</title>")
    description = re.search(r'<meta name="description" content="([^"]*)">', page).group(1)
    if len(html.unescape(description)) > DESCRIPTION_MAX:
        page = page.replace(description, description.replace(esc(joined(names)), "your retro handheld"))
    for place, part in (("hero", hero(out, devices)), ("facts", facts(devices)), ("fact-notes", fact_notes(devices)),
                        ("install", install(devices)), ("after-install", after_install(devices)),
                        ("faq", faq(devices)), ("data", rules(devices) + "\n" + data(out, devices))):
        token = f"<!--devices:{place}-->"
        if page.count(token) != 1:
            fail(f"site/index.html needs one {token}")
        page = page.replace(token, part)
    page = marks(page, devices)
    page = systems(page, devices)
    page = lines(page, devices)
    if "{{device" in page or "<!--devices:" in page:
        fail("a device placeholder left in the page")
    page_path.write_text(page, encoding="utf-8")
    print(f"devices: {', '.join(d['id'] for d in devices)}")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--devices", type=Path, default=ROOT / "site/devices.json")
    a = ap.parse_args()
    build(a.out, a.devices)
