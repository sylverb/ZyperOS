#!/usr/bin/env python3
"""Your PICO-8's Shader and Border, drawn (make check-p8fx-gl).

    python3 tools/p8fx-gl-check.py            on Linux with SDL2 and Mesa
    python3 tools/p8fx-gl-check.py --docker   anywhere with Docker: the same,
                                              in tools/p8fx-gl.Dockerfile's Linux

The shim (src/p8fx/shim.c, LD_PRELOAD) over a stand-in that draws as
pico8_64 0.2.7 does (tools/p8fx-standin.c), on SDL2's offscreen video with
its OpenGL ES 2 renderer on Mesa's llvmpipe, each frame read back before the
present. PICO-8 itself never runs here: it is the player's, and only the
handheld has it. This fails when:
  - with the shim and no look file, the look Off, a shader the shim has not
    (LCD No Backlight, a name of none), a border picture that cannot be
    read, a GL below OpenGL ES 3, or a renderer that is not OpenGL ES's,
    the frame is not the very frame drawn without it (fail-safe);
  - a shader's picture, inside PICO-8's rect, is not what Diatom's GLSL makes
    of the same 128x128 picture at that size (tools/shaderdemo.c --image),
    at Full Screen (preblit 3, smooth), at Sharp Pixels (preblit 1, sharp),
    under 100% and for a rect past the screen's edges (Zoomed);
  - round the rect, a shader leaves anything but black; Blur leaves black,
    or a colour brighter than the picture's; a picture border is not the
    picture; and inside, with Off, the picture is not SDL's own;
  - PICO-8's FPS line, which SDL draws after the screen's copy, is not as
    without the shim (the GL state SDL keeps was not put back);
  - a look written while the stand-in runs is not the one drawn next.
Skips, saying so, where it cannot run: not Linux and no --docker, no SDL2 or
SDL2_image to build with, no OpenGL ES renderer under SDL's offscreen video.
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMAGE = "zyperos-p8fx-gl"
W, H = 640, 480
# Diatom's GLSL on a GPU against its C on the CPU: the same arithmetic in
# float, so a level of difference and a rare second from a different
# rounding of the same number
TOL = 3


def skip(why):
    print("skipped: " + why)
    return 0


def docker():
    if not shutil.which("docker"):
        return skip("no Docker for --docker")
    if subprocess.run(["docker", "image", "inspect", IMAGE], capture_output=True).returncode:
        print("p8fx-gl: building %s (tools/p8fx-gl.Dockerfile, once)" % IMAGE)
        if subprocess.run(["docker", "build", "-q", "-t", IMAGE, "-f",
                           str(ROOT / "tools/p8fx-gl.Dockerfile"), str(ROOT / "tools")]).returncode:
            return skip("could not build the %s image" % IMAGE)
    return subprocess.run(["docker", "run", "--rm", "-v", "%s:/z:ro" % ROOT, "-w", "/z", IMAGE,
                           "python3", "tools/p8fx-gl-check.py"]).returncode


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)


def ppm(path):
    data = Path(path).read_bytes()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", data)
    w, h = int(m.group(1)), int(m.group(2))
    return w, h, data[m.end():]


def px(img, x, y):
    w, _, d = img
    i = (y * w + x) * 3
    return d[i], d[i + 1], d[i + 2]


def dtov(path, w, h, rgb_at):
    """A DTOV picture (Diatom's overlay file): BGRA rows"""
    rows = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b = rgb_at(x, y)
            rows += bytes((b, g, r, 255))
    Path(path).write_bytes(b"DTOV" + struct.pack("<HH", w, h) + bytes(rows))


def main():
    if sys.argv[1:] == ["--docker"]:
        return docker()
    if sys.platform != "linux":
        return skip("needs Linux (LD_PRELOAD, Mesa's OpenGL ES); python3 tools/p8fx-gl-check.py --docker")
    if sh("pkg-config --exists sdl2 SDL2_image").returncode:
        return skip("no SDL2 and SDL2_image to build with")
    cc = os.environ.get("CC", "cc")
    tmp = Path(tempfile.mkdtemp(prefix="p8fx-gl."))
    try:
        return run(cc, tmp)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def run(cc, B):
    # The shim from the payload's own list of files (mk/cross.mk, P8FX_SRC)
    m = re.search(r"^P8FX_SRC := (.*)$", (ROOT / "mk/cross.mk").read_text(), re.M)
    if not m:
        print("FAIL mk/cross.mk has no P8FX_SRC")
        return 1
    builds = [
        "%s -shared -fPIC -fvisibility=hidden -O2 -Wall -Wextra -Wno-unused-parameter -std=gnu11 -D_GNU_SOURCE "
        "-Isrc -Idiatom/include -Idiatom/src $(pkg-config --cflags sdl2) -o %s/p8fx.so %s -ldl"
        % (cc, B, m.group(1)),
        "%s -O2 -Wall -Wextra -std=gnu11 -o %s/standin tools/p8fx-standin.c $(pkg-config --cflags --libs sdl2)"
        % (cc, B),
        "%s -std=gnu11 -O2 -Wall -Wextra $(pkg-config --cflags sdl2 SDL2_image) -o %s/shaderdemo "
        "tools/shaderdemo.c $(pkg-config --libs sdl2 SDL2_image) -lm" % (cc, B),
    ]
    for b in builds:
        r = sh(b, cwd=ROOT)
        if r.returncode:
            print("FAIL could not build: %s\n%s" % (b, r.stderr))
            return 1
    env0 = dict(os.environ, SDL_VIDEODRIVER="offscreen", SDL_RENDER_DRIVER="opengles2")
    look = B / "look"
    bad = []
    logs = []

    def frame(name, args=(), shim=True, env=None, text=None):
        """The stand-in's last frame, with the shim and the look `text`"""
        e = dict(env0, **(env or {}))
        if shim:
            e["LD_PRELOAD"] = str(B / "p8fx.so")
            e["ZYPEROS_P8FX"] = str(look)
        if text is None:
            look.unlink(missing_ok=True)
        else:
            look.write_text(text)
        out = B / (name + ".ppm")
        r = subprocess.run([str(B / "standin"), *args, "--scene", str(B / "scene.ppm"), str(out)],
                           env=e, capture_output=True, text=True)
        logs.append("%s: %s" % (name, r.stderr.strip()))
        if r.returncode:
            raise SystemExit("FAIL the stand-in (%s): %s" % (name, r.stderr.strip()))
        return ppm(out)

    def lookt(shader="nearest", strength=100, border="off", border_file=None):
        t = "shader %s\nstrength %d\nborder %s\n" % (shader, strength, border)
        return t + ("border_file %s\n" % border_file if border_file else "")

    def same(name, a, b, box=(0, 0, W, H)):
        x0, y0, x1, y1 = box
        for y in range(y0, y1):
            for x in range(x0, x1):
                if px(a, x, y) != px(b, x, y):
                    bad.append("%s: (%d, %d) is %s, %s without the shim" % (name, x, y, px(a, x, y), px(b, x, y)))
                    return False
        return True

    def like_cpu(name, img, rect, shader, strength):
        """Inside the rect, against Diatom's GLSL in C at the rect's size"""
        x, y, w, h = rect
        ref = B / ("ref-%s.ppm" % name)
        r = subprocess.run([str(B / "shaderdemo"), "--image", str(B / "scene.ppm"), shader, str(strength),
                            str(w), str(h), str(ref)], capture_output=True, text=True)
        if r.returncode:
            bad.append("%s: shaderdemo --image: %s" % (name, r.stderr.strip()))
            return
        cpu = ppm(ref)
        worst, at = 0, None
        for yy in range(max(0, y), min(H, y + h)):
            for xx in range(max(0, x), min(W, x + w)):
                d = max(abs(p - q) for p, q in zip(px(img, xx, yy), px(cpu, xx - x, yy - y)))
                if d > worst:
                    worst, at = d, (xx, yy)
        if worst > TOL:
            bad.append("%s: %s at %d%% is %d levels off Diatom's GLSL in C at (%d, %d)"
                       % (name, shader, strength, worst, at[0], at[1]))
        return worst

    def black_outside(name, img, rect):
        x, y, w, h = rect
        for yy in range(H):
            for xx in range(W):
                if not (x <= xx < x + w and y <= yy < y + h) and px(img, xx, yy) != (0, 0, 0):
                    bad.append("%s: (%d, %d) round the picture is %s, not black" % (name, xx, yy, px(img, xx, yy)))
                    return

    full = (80, 0, 480, 480)
    sharp = (128, 48, 384, 384)
    # SDL's offscreen video with its OpenGL ES 2 renderer, or nothing here
    # to check with (a Linux with no Mesa)
    probe = subprocess.run([str(B / "standin"), str(B / "probe.ppm")], env=env0, capture_output=True, text=True)
    if probe.returncode or "p8fx-standin: renderer opengles2" not in probe.stderr:
        return skip("no OpenGL ES 2 renderer under SDL's offscreen video here (%s)" % probe.stderr.strip())
    base = frame("base", shim=False)
    base_sharp = frame("base-sharp", ["--sharp"], shim=False)
    base_fps = frame("base-fps", ["--fps"], shim=False)
    checked = 0

    # Fail-safe: SDL's own frame, to the pixel
    for name, args, ref, kw in [
        ("no look file", [], base, {}),
        ("Off", [], base, {"text": lookt()}),
        ("Off, Sharp Pixels", ["--sharp"], base_sharp, {"text": lookt()}),
        ("a shader of none", [], base, {"text": lookt("vhs")}),
        ("LCD No Backlight, the Game Boys'", [], base, {"text": lookt("lcd-unlit")}),
        ("a border picture not there", [], base, {"text": lookt(border="picture", border_file="/nonexistent.dtov")}),
        ("OpenGL ES 2 only", [], base, {"text": lookt("lcd", border="blur"),
                                        "env": {"MESA_GLES_VERSION_OVERRIDE": "2.0"}}),
    ]:
        same(name, frame(name.replace(" ", "-"), args, **kw), ref)
        checked += 1
    soft = frame("software", shim=False, env={"SDL_RENDER_DRIVER": "software"})
    same("the software renderer", frame("software-shim", env={"SDL_RENDER_DRIVER": "software"},
                                        text=lookt("lcd", border="blur")), soft)
    checked += 1

    # Each shader, inside the rect against the C, black round it
    worst = 0
    for shader, strength, args, rect in [
        ("shimmerless", 100, [], full), ("crt", 100, [], full), ("crt-curved", 100, [], full),
        ("scanlines", 100, [], full), ("lcd", 100, [], full), ("crt", 50, [], full), ("lcd", 30, [], full),
        ("lcd", 100, ["--sharp"], sharp), ("crt", 100, ["--sharp"], sharp),
        ("scanlines", 100, ["--sharp", "--preblit", "2"], sharp),
        ("lcd", 100, ["--rect", "64,-16,512,512"], (64, -16, 512, 512)),
        # src/p8draw.h's Full Screen (the panel, its shape changed), Zoomed
        # (5x, cut) and Actual Size (1x, sharp)
        ("crt", 100, ["--rect", "0,0,640,480"], (0, 0, 640, 480)),
        ("scanlines", 100, ["--sharp", "--rect", "0,-80,640,640"], (0, -80, 640, 640)),
        ("lcd", 100, ["--sharp", "--rect", "256,176,128,128"], (256, 176, 128, 128)),
    ]:
        name = "%s-%d%s" % (shader, strength, "".join(args).replace(",", "_"))
        img = frame(name, args, text=lookt(shader, strength))
        worst = max(worst, like_cpu(name, img, rect, shader, strength) or 0)
        black_outside(name, img, rect)
        checked += 1

    # Blur: SDL's picture inside, the blur round it, never black, never
    # brighter than the picture's brightest
    img = frame("blur", text=lookt(border="blur"))
    same("Blur, inside", img, base, (80, 0, 560, 480))
    outside = [px(img, x, y) for y in range(0, H, 7) for x in list(range(0, 80, 5)) + list(range(560, W, 5))]
    top = max(max(px(base, x, y)) for y in range(0, H, 5) for x in range(80, 560, 5))
    if all(c == (0, 0, 0) for c in outside):
        bad.append("Blur: black round the picture")
    if max(max(c) for c in outside) > top:
        bad.append("Blur: brighter round the picture than the picture")
    checked += 1
    # A picture: the picture round SDL's own
    pic = B / "border.dtov"
    dtov(pic, W, H, lambda x, y: ((x * 255) // (W - 1), (y * 255) // (H - 1), 128))
    img = frame("picture", text=lookt(border="picture", border_file=str(pic)))
    same("a picture border, inside", img, base, (80, 0, 560, 480))
    for x, y in [(0, 0), (79, 240), (560, 0), (639, 479), (20, 400)]:
        want = ((x * 255) // (W - 1), (y * 255) // (H - 1), 128)
        if max(abs(p - q) for p, q in zip(px(img, x, y), want)) > 1:
            bad.append("a picture border: (%d, %d) is %s, the picture's %s" % (x, y, px(img, x, y), want))
    checked += 1
    # Blur where the picture covers the screen: no room, SDL's own frame
    same("Blur at Full Screen", frame("blur-full", ["--rect", "0,0,640,480"], text=lookt(border="blur")),
         frame("base-full", ["--rect", "0,0,640,480"], shim=False))
    checked += 1
    # A shader and a border at once
    img = frame("lcd-blur", text=lookt("lcd", border="blur"))
    like_cpu("lcd and Blur", img, full, "lcd", 100)
    if px(img, 10, 10) == (0, 0, 0):
        bad.append("LCD Grid and Blur: no blur round the picture")
    checked += 1

    # PICO-8's FPS line over a shader, as SDL draws it without the shim
    img = frame("fps", ["--fps"], text=lookt("crt", 50, border="blur"))
    same("the FPS line over CRT and Blur", img, base_fps, (0, 0, 80, 8))
    checked += 1

    # A look written while it runs: Off at the first frame, LCD Grid after
    nxt = B / "look.next"
    nxt.write_text(lookt("lcd"))
    img = frame("live", ["--frames", "2", "--then", str(nxt)], text=lookt())
    like_cpu("a look written while it runs", img, full, "lcd", 100)
    checked += 1

    for b in bad:
        print("FAIL " + b)
    if bad:
        print("\n".join(logs))
        return 1
    print("ok: %d frames drawn with the shim: fail-safe to the pixel, each shader within %d level%s of "
          "Diatom's GLSL in C, Blur and a picture round the picture, PICO-8's FPS line on top, a look "
          "written while it runs drawn next" % (checked, worst, "" if worst == 1 else "s"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
