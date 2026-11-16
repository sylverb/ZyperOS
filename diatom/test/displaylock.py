"""The display lock (src/pace.h), end to end against the stub core.

The stub runs at 59.7275 fps, a GBA's rate. With --pace smooth (RUN
pace=smooth, the launcher's Timing: Smooth) and a display rate within 1% of
it (DIATOM_REFRESH_HZ, the desktop port's stand-in for a panel's mode), the
game runs at the display's, one frame a refresh, and says so; without it
(Exact), or with no display rate, at its own, as before. Diatom's summary
gives the frames and the time.

  - --pace smooth, DIATOM_REFRESH_HZ=60: "locked to the display", 60 a second
  - the same with no --pace (Exact): not locked, the stub's 59.73
  - --pace exact: not locked either
  - --pace smooth, no display rate: not locked, 59.73
  - --pace smooth, a 50 Hz display, more than 1% from the core: not locked

A busy machine can run a loop late, and late frames make any rate lower, so
a rate is judged with room: locked within 1% of 60, and at least 0.15 fps
over the core's own rate from the same run's conditions; not locked within 1%
of 59.7275 and under 60.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-now does. SDL's dummy drivers: no window, no sound.

    python3 test/displaylock.py
"""
import os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-lock-")
ROM = f"{TMP}/A Game (World).stub"
with open(ROM, "wb") as f:
    f.write(b"STUB" + bytes(range(60)))

fails = []
def check(name, ok, got):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok: fails.append(name)

def run(refresh=None, extra=()):
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIODRIVER="dummy", STUBCORE_FRAME_US="2000")
    env.pop("DIATOM_REFRESH_HZ", None)
    if refresh is not None:
        env["DIATOM_REFRESH_HZ"] = str(refresh)
    out = subprocess.run([BIN, "--core", STUB, "--rom", ROM, "--save", TMP, "--system", TMP,
                          "--frames", "480", *extra], env=env, capture_output=True, text=True,
                         timeout=60).stdout
    m = re.search(r"(\d+) frames in ([\d.]+)s = ([\d.]+) fps \(target ([\d.]+)\)", out)
    fps = float(m.group(3)) if m else 0.0
    target = float(m.group(4)) if m else 0.0
    return out, fps, target

SMOOTH = ("--pace", "smooth")
out, fps, target = run(60, SMOOTH)
check("Smooth on a 60 Hz display: locked, and says so", "locked to the display, 60.000 Hz" in out, out[-600:])
check("  ...its frames' target the display's 60", abs(target - 60.0) < 0.001, target)
check("  ...and run at it, within 1%", abs(fps - 60.0) / 60.0 < 0.01, fps)
out_c, fps_c, target_c = run(60)
check("no --pace (Exact): not locked, and nothing said", "locked to the display" not in out_c and
      "not the display's" not in out_c, out_c[-600:])
check("  ...the core's own 59.7275", abs(target_c - 59.7275) < 0.001, target_c)
check("  ...and slower than locked, by the 0.46%", fps - fps_c >= 0.15 and fps_c < 60.0, (fps, fps_c))
out_e, fps_e, target_e = run(60, ("--pace", "exact"))
check("--pace exact: not locked either", "locked to the display" not in out_e and abs(target_e - 59.7275) < 0.001,
      (target_e, out_e[-300:]))
out_n, fps_n, target_n = run(None, SMOOTH)
check("Smooth, no display rate: not locked, as before", "locked to the display" not in out_n and abs(target_n - 59.7275) < 0.001,
      (target_n, out_n[-300:]))
out_p, fps_p, target_p = run(50, SMOOTH)
check("Smooth on a 50 Hz display, 16% from the core: not locked", "more than 1% apart" in out_p and abs(target_p - 59.7275) < 0.001,
      (target_p, out_p[-300:]))

if fails:
    print(f"displaylock: {len(fails)} FAILED")
    sys.exit(1)
print("ok: with pace=smooth a core within 1% of the display's rate runs at it and says so; without it, no rate or a far one, its own")
