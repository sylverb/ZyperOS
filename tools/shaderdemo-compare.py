#!/usr/bin/env python3
"""A Pixel 2's shots of each filter beside what tools/shaderdemo.c says it draws.

    python3 tools/shaderdemo-compare.py DIR     (tools/shaderdemo-device.sh runs it)

DIR/want holds tools/shaderdemo.c --rgb565's pictures (the whole 640x480 panel,
from the scene as a core hands it over), DIR/shot the device's panel shots
of the same scene through Diatom's filters (px2-shot's PPMs), by the same names.
For each it says how far the two are apart in 8-bit levels (each channel of
each pixel), and writes DIR/sheet-<name>.png: the top left of the panel, what
the Shader list shows of it, at 2x, the picture, the shot and the difference
eight times over. They are one picture when the mean is at most 1 level and
99.5% of the values are within 3: the GPU's float and the scanout's own
rounding. A Pixel 2 on 1.0.5 (2026-11-16) gave a mean of 0.28 to 0.36 and
99.5% within 1 for all ten; a picture one pixel off, or two levels off
everywhere, fails.
"""
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

MEAN_MAX = 1.0
P995_MAX = 3
CROP = (0, 0, 190, 142)


def main():
    d = Path(sys.argv[1])
    bad = 0
    rows = []
    for want_p in sorted((d / "want").glob("*.png")):
        name = want_p.stem
        shot_p = d / "shot" / (name + ".ppm")
        if not shot_p.exists():
            print("FAIL %s: no shot" % name)
            bad += 1
            continue
        want = Image.open(want_p).convert("RGB")
        shot = Image.open(shot_p).convert("RGB")
        if want.size != shot.size:
            print("FAIL %s: the shot is %dx%d, the picture %dx%d" % ((name,) + shot.size + want.size))
            bad += 1
            continue
        diff = ImageChops.difference(want, shot)
        hist = [0] * 256
        for band in diff.split():
            for i, n in enumerate(band.histogram()):
                hist[i] += n
        total = sum(hist)
        mean = sum(i * n for i, n in enumerate(hist)) / total
        run, p995 = 0, 0
        for i, n in enumerate(hist):
            run += n
            if run >= 0.995 * total:
                p995 = i
                break
        top = max(i for i, n in enumerate(hist) if n)
        ok = mean <= MEAN_MAX and p995 <= P995_MAX
        bad += not ok
        print("%s %-16s mean %.2f, 99.5%% within %d, most %d" % ("ok  " if ok else "FAIL", name, mean, p995, top))
        rows.append(name)
        # The sheet: what the Shader list shows of it, at 2x
        w, h = CROP[2] * 2, CROP[3] * 2
        sheet = Image.new("RGB", (w * 3 + 16, h + 20), (255, 255, 255))
        loud = diff.crop(CROP).point(lambda v: min(255, v * 8))
        for i, im in enumerate((want.crop(CROP), shot.crop(CROP), loud)):
            sheet.paste(im.resize((w, h), Image.NEAREST), (i * (w + 8), 20))
        ImageDraw.Draw(sheet).text((2, 4), "%s: picture | Pixel 2 | difference x8   mean %.2f, 99.5%% within %d"
                                   % (name, mean, p995), fill=(0, 0, 0))
        sheet.save(d / ("sheet-%s.png" % name))
    if not rows:
        print("FAIL nothing to compare in %s" % d)
        return 1
    print("shaderdemo-compare: %d of %d the same picture" % (len(rows) - bad, len(rows)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
