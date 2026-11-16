#!/usr/bin/env python3
"""The Shader list's pictures (src/shaderdemo.h) are Diatom's own filters.

    python3 tools/shaderdemo-check.py           check (make check-shaderdemo)
    python3 tools/shaderdemo-check.py --write   after make shaderdemo: record
                                                the filters' sources it was made from

res/shaderdemo/ holds, for each panel, every filter's picture and each old
screen's Strength steps, made by tools/shaderdemo.c from Diatom's filters in C.
This fails when:
  - the pictures are not what tools/shaderdemo.c makes now (made again into a
    folder of its own and compared pixel for pixel);
  - a filter the Shader rows offer (SHADERS, src/app.h) has a picture missing;
  - Diatom's filters changed since the pictures were made: its GLSL
    (diatom/port/pixel2.c, Sharp-Shimmerless's pass through the CRT's) or the
    Brick's Sharp-Shimmerless on its CPU (diatom/port/brick.c). Port the change
    into tools/shaderdemo.c, run make shaderdemo, look at the pictures, set
    them beside a Pixel 2's (tools/shaderdemo-device.sh), and record them
    (--write).
"""
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEMO = ROOT / "res" / "shaderdemo"
SOURCES = DEMO / "SOURCES"
TOOL = ROOT / "build-native" / "shaderdemo"
PANELS = ["640x480", "1024x768"]


def between(path, start, end):
    """The text of `path` from the line with `start` to the line before `end`"""
    lines = path.read_text(encoding="utf-8").splitlines()
    a = next(i for i, l in enumerate(lines) if start in l)
    b = next(i for i, l in enumerate(lines) if i > a and end in l)
    return "\n".join(lines[a:b])


def brick_block(path):
    """The Brick's CPU Sharp-Shimmerless: its branch in the column map"""
    lines = path.read_text(encoding="utf-8").splitlines()
    a = next(i for i, l in enumerate(lines) if "if (filter >= DIATOM_FILTER_SHIMMERLESS) {" in l)
    b = next(i for i, l in enumerate(lines) if i > a and l.strip() == "continue;")
    return "\n".join(lines[a:b + 1])


def sources():
    """What the pictures stand for: each part's name and its text's sha256"""
    parts = {
        "diatom/port/pixel2.c (the filters' GLSL)":
            between(ROOT / "diatom/port/pixel2.c", "static const char *SHIMMERLESS_SHADER",
                    "static const char *SOLID_SHADER"),
        "diatom/port/brick.c (Sharp-Shimmerless on the CPU)": brick_block(ROOT / "diatom/port/brick.c"),
    }
    return {k: hashlib.sha256(v.encode("utf-8")).hexdigest() for k, v in parts.items()}


def offered():
    """The SHADERS' filter names, and which have a Strength row (the gpu ones)"""
    text = (ROOT / "src/app.h").read_text(encoding="utf-8")
    return re.findall(r'\{\s*"([a-z-]+)",\s*S_[A-Z_]+,\s*(true|false)\s*\}', text)


def main():
    write = sys.argv[1:] == ["--write"]
    bad = []
    if write:
        SOURCES.write_text("# What res/shaderdemo was made from (tools/shaderdemo-check.py --write)\n" +
                           "".join("%s  %s\n" % (h, k) for k, h in sources().items()), encoding="utf-8")
        print("wrote %s" % SOURCES.relative_to(ROOT))
        return 0
    # Diatom's filters, as when the pictures were made
    kept = {}
    if SOURCES.exists():
        for line in SOURCES.read_text(encoding="utf-8").splitlines():
            if line and not line.startswith("#"):
                h, k = line.split("  ", 1)
                kept[k] = h
    for k, h in sources().items():
        if kept.get(k) != h:
            bad.append("%s changed since res/shaderdemo was made: port it into tools/shaderdemo.c, "
                       "make shaderdemo, look, compare on a Pixel 2 (tools/shaderdemo-device.sh), and record it "
                       "(tools/shaderdemo-check.py --write)" % k)
    # Every offered filter's pictures
    for panel in PANELS:
        for name, gpu in offered():
            want = [name + ".png"] + ([name + "-%d.png" % s for s in range(10, 100, 10)] if gpu == "true" else [])
            for f in want:
                if not (DEMO / panel / f).exists():
                    bad.append("res/shaderdemo/%s/%s is missing" % (panel, f))
    # The pictures are what the tool makes now
    if TOOL.exists():
        from PIL import Image, ImageChops

        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([str(TOOL), tmp], check=True)
            for panel in PANELS:
                made = sorted(p.name for p in (Path(tmp) / panel).glob("*.png"))
                held = sorted(p.name for p in (DEMO / panel).glob("*.png"))
                if made != held:
                    bad.append("res/shaderdemo/%s holds %d pictures, the tool makes %d" % (panel, len(held), len(made)))
                for name in sorted(set(made) & set(held)):
                    a = Image.open(Path(tmp) / panel / name).convert("RGB")
                    b = Image.open(DEMO / panel / name).convert("RGB")
                    if a.size != b.size or ImageChops.difference(a, b).getbbox():
                        bad.append("res/shaderdemo/%s/%s is not what tools/shaderdemo.c makes: make shaderdemo"
                                   % (panel, name))
    else:
        bad.append("no build-native/shaderdemo to make the pictures again with")
    for b in bad:
        print("FAIL " + b)
    if bad:
        return 1
    print("shaderdemo: %d pictures, each what Diatom's filters make of the scene, for every filter offered"
          % sum(len(list((DEMO / p).glob("*.png"))) for p in PANELS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
