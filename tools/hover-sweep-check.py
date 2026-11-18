#!/usr/bin/env python3
"""What Theme's lists' cursors show writes nothing (src/ds/hover.h).

    python3 tools/hover-sweep-check.py        (make check-hover-sweep)

Each card below is shot with --hover-sweep: Theme's Accent, Light Background
and Dark Background show every choice and put the kept one back, as their
lists would, and three frames of the shelf behind are drawn after each show.
The launcher says, a line a list, how many it showed, how many database rows
were written meanwhile, whether the page reads the same after, and the
slowest show and frame (src/hover_sweep.h). This fails on a row written, a
page that reads otherwise after, or a list that should show and is missing;
and on any file the sweep left on the card that the same shot without it
does not (the theme file, the boot screen's words). It says the slowest
show and frame here, a Mac's, not the Pixel 2's.
"""
import hashlib
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("rs", ROOT / "tools" / "readme-shots.py")
R = importlib.util.module_from_spec(spec)
spec.loader.exec_module(R)

THEME = {("theme", "Accent"), ("theme", "Light Background"), ("theme", "Dark Background")}
# (name, device, settings)
CASES = [
    ("pixel2 light", "pixel2", {}),
    ("pixel2 dark", "pixel2", {"appearance": "dark"}),
    ("brick", "brick", {}),
]


def tree(card):
    """Every file on the card but the databases and logs: its path and hash"""
    out = {}
    for p in card.rglob("*"):
        if not p.is_file():
            continue
        if ".db" in p.name or p.name.endswith(".log") or "/logs/" in p.as_posix():
            continue
        out[p.relative_to(card).as_posix()] = hashlib.sha1(p.read_bytes()).hexdigest()
    return out


def shoot(device, settings, sweep, tmp):
    card = Path(tmp) / ("sweep" if sweep else "plain")
    R.demo_card(card, None, {"artwork": {}})
    (card / "ZyperOS/.last").write_text("PS1\n" + R.FOCUS + ".chd\n")
    R.set_settings(card, settings, True)
    cmd = [str(ROOT / "build-native/zyperos"), "--shot", str(Path(tmp) / "s.png"), "--lang", "en",
           "--screen", "systems", "--status", "10:08 24h wifi=off batt=82", "--device", device]
    if sweep:
        cmd.append("--hover-sweep")
    r = subprocess.run(cmd, cwd=ROOT, env=R.env_for(card), text=True, errors="replace",
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    return r, tree(card)


def main():
    binary = ROOT / "build-native" / "zyperos"
    if subprocess.call(["pkg-config", "--exists", "sdl2", "SDL2_image", "SDL2_ttf"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
        print("skipped: no SDL2 here to build the launcher with")
        return 0
    if not binary.is_file():
        print("FAIL no build-native/zyperos: make native first")
        return 1
    fails, lists, show_ms, frame_ms = [], 0, 0.0, 0.0
    for name, device, settings in CASES:
        with tempfile.TemporaryDirectory() as tmp:
            _, plain_tree = shoot(device, settings, False, tmp)
            r, sweep_tree = shoot(device, settings, True, tmp)
        if r.returncode:
            fails.append("%s: the launcher ended with %d: %s" % (name, r.returncode, r.stdout[-400:]))
            continue
        seen = set()
        for line in r.stdout.splitlines():
            if not line.startswith("hover: "):
                continue
            page, label, shown, written, same, s_ms, f_ms = line[7:].split("\t")
            seen.add((page, label))
            lists += 1
            show_ms, frame_ms = max(show_ms, float(s_ms)), max(frame_ms, float(f_ms))
            if int(written):
                fails.append("%s: %s > %s wrote %s database row(s) while its cursor showed" %
                             (name, page, label, written))
            if same != "same":
                fails.append("%s: %s > %s left its page reading otherwise" % (name, page, label))
            if int(shown) <= 0:
                fails.append("%s: %s > %s showed nothing" % (name, page, label))
        for page, label in sorted(THEME - seen):
            fails.append("%s: %s > %s did not show its choices" % (name, page, label))
        for path in sorted(set(sweep_tree) - set(plain_tree)):
            fails.append("%s: the sweep wrote %s" % (name, path))
        for path in sorted(p for p in set(sweep_tree) & set(plain_tree) if sweep_tree[p] != plain_tree[p]):
            fails.append("%s: the sweep changed %s" % (name, path))
    for f in fails:
        print("FAIL " + f)
    if fails:
        return 1
    print("ok: Theme's %d lists in %d cases show every choice and write nothing, and the page reads the same "
          "after; here the slowest show takes %.2f ms and the slowest frame after it %.2f ms"
          % (lists, len(CASES), show_ms, frame_ms))
    return 0


if __name__ == "__main__":
    sys.exit(main())
