"""Refreshes missed with the frame ready, end to end against the stub core
(src/flipwatch.h).

On the GKD Pixel 2 a shader on a GPU capped by a system's power line made
every flip take two refreshes while the core kept its time (Sonic on the
Master System, 30 new frames a second, 2026-11-16), and nothing lifted the
cap. The desktop port counts every frame after the N-th as such a miss
(DIATOM_TEST_MISSED=N, port/desktop.c) and writes the ceilings and the holds
into DIATOM_POWER_DIR as the Pixel 2 writes its clocks:

  - power light, misses from the start's third second: at full power the
    CPU and the GPU held at their tops, said once, and the summary counts
    the misses (on a desktop the level reaches full on the stub's sound,
    which has no device and counts as dropped; the level step on a miss is
    flipwatch_test.c's)
  - the holds let go when the game ends
  - no misses: no hold and no count
  - power full, misses: held at once

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-power-e2e does. SDL's dummy drivers: no window, no sound.

    python3 test/flipwatch.py
"""
import os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-flipwatch-")
ROM = f"{TMP}/A Game (World).stub"
with open(ROM, "wb") as f:
    f.write(b"STUB" + bytes(range(60)))

fails = []
def check(name, ok, got):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok: fails.append(name)

def run(power, missed=None, frames=540):
    pdir = tempfile.mkdtemp(dir=TMP)
    for n in ("ram_max_freq", "gpu_max_freq"):
        open(f"{pdir}/{n}", "w").close()
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIODRIVER="dummy", STUBCORE_FRAME_US="2000", DIATOM_POWER_DIR=pdir)
    env.pop("DIATOM_TEST_MISSED", None)
    if missed is not None:
        env["DIATOM_TEST_MISSED"] = str(missed)
    out = subprocess.run([BIN, "--core", STUB, "--rom", ROM, "--save", TMP, "--system", TMP,
                          "--frames", str(frames), "--power", power], env=env,
                         capture_output=True, text=True, timeout=60).stdout
    holds = {}
    for n in ("cpu_hold", "gpu_hold"):
        p = f"{pdir}/{n}"
        holds[n] = open(p).read() if os.path.exists(p) else None
    return out, holds

def lines(out, word):
    return [l for l in out.splitlines() if word in l]

# light, misses from the 180th frame on (3 s in). On a desktop the stub's
# sound has no device to go to and counts as dropped, which lifts the level
# to full in the first seconds by itself ("power up ... after dropped
# sound"); the level step on a miss is test/flipwatch_test.c's. Here: the
# hold, said once, after full, and the holds' files.
HELD = "the CPU and the GPU held at their tops after a refresh missed"
out, holds = run("light", missed=180)
said = [l for l in out.splitlines() if "power up" in l or HELD in l]
check("a light game ends up at full power", any("up to full" in l for l in said), said or out[-600:])
check("  ...then the CPU and the GPU held at their tops, once", len(lines(out, HELD)) == 1, said)
held_at = [i for i, l in enumerate(said) if HELD in l]
full_at = [i for i, l in enumerate(said) if "up to full" in l]
check("  ...after full", held_at and full_at and full_at[0] < held_at[0], said)
m = re.search(r"(\d+) refresh\(es\) missed with the frame ready; the CPU and the GPU held at their tops", out)
check("  ...and the summary counts them", m is not None and int(m.group(1)) > 300, out[-400:])
check("the holds let go at the game's end", holds["cpu_hold"] == "0" and holds["gpu_hold"] == "0", holds)

# no misses: nothing
out, holds = run("light")
check("no misses: no hold and no count", "refresh missed" not in out and "held at their tops" not in out and
      "missed with the frame ready" not in out, out[-600:])

# full from the start: held at once
out, holds = run("full", missed=180)
check("a full game: the CPU and the GPU held, once", len(lines(out, HELD)) == 1 and
      "power up" not in out, out[-600:])

if fails:
    print(f"flipwatch e2e: {len(fails)} FAILED")
    sys.exit(1)
print("ok: refreshes missed with the frame ready lift the level, then hold the CPU and the GPU; none, nothing")
