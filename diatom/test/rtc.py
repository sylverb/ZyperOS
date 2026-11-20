"""A cartridge's clock kept apart from its battery RAM (RETRO_MEMORY_RTC),
end to end against the stub core: Gambatte keeps an MBC3's clock there
(Pokemon Gold, Silver and Crystal), and Diatom keeps it as RetroArch does,
<rom>.rtc beside the .srm, read in at the start and written on the way out
(src/save.c). The stub's clock (STUBCORE_RTC) counts the frames it ran, so:

  - a run writes <rom>.rtc, its count the frames it ran;
  - the next run reads it in, and its count goes on from there;
  - a core that keeps no clock apart writes none;
  - a file of another size is the core's layout, and is not read.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-proto does. SDL's dummy drivers: no window, no sound.

    python3 test/rtc.py
"""
import os, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-rtc-")
fails = []


def check(name, ok, got=None):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok:
        fails.append(name)


def run(frames, rtc=True):
    rom = f"{TMP}/Pocket Monsters.gb"
    if not os.path.exists(rom):
        open(rom, "wb").write(b"\0" * 32768)
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software", SDL_AUDIODRIVER="dummy")
    if rtc:
        env["STUBCORE_RTC"] = "1"
    else:
        env.pop("STUBCORE_RTC", None)
    p = subprocess.run([BIN, "--core", STUB, "--rom", rom, "--save", TMP, "--system", TMP,
                        "--frames", str(frames)], env=env, stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, text=True, errors="replace", timeout=60)
    return p.stderr


def count():
    path = f"{TMP}/Pocket Monsters.rtc"
    if not os.path.exists(path):
        return None
    data = open(path, "rb").read()
    return struct.unpack("<I", data[:4])[0] if len(data) == 16 else -len(data)


print("a cartridge's clock, apart from its battery:")
try:
    run(30)
    first = count()
    check("a run writes <rom>.rtc, its count the frames run", first is not None and first >= 30, first)
    log = run(20)
    second = count()
    check("the next run reads it in and goes on from it", second is not None and first is not None and
          second >= first + 20 and "the cartridge's clock from" in log, (first, second))
    os.remove(f"{TMP}/Pocket Monsters.rtc")
    run(10, rtc=False)
    check("a core that keeps no clock apart writes none", count() is None, count())
    open(f"{TMP}/Pocket Monsters.rtc", "wb").write(b"\x40\x42\x0f\x00" + b"\0" * 4)
    log = run(10)
    got = count()
    check("a file of another size is not read, and the run's own clock is written",
          got is not None and 0 < got < 1000000 and "not read" in log, got)
finally:
    shutil.rmtree(TMP, ignore_errors=True)

if fails:
    print(f"\n{len(fails)} rtc check(s) failed")
    sys.exit(1)
print("\nok: the clock goes on across runs, kept beside the save")
