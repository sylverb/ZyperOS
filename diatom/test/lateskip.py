"""frameskip=late and frameskip=steady at 1x, end to end against the stub core.

A DS game sent with frameskip=late leaves out the drawing of a frame that
starts late, two in a row at most, so a slow stretch shows frames 16.7 and
33.3 ms apart in turn (Mario Kart DS on the Pixel 2: a median 41 fps, uneven,
2026-11-16). frameskip=steady, the player's When Slow: Steady 30, draws
exactly every second frame while frames start late, and every frame again
once they have started on time for half a second (main.c, STEADY_HOLD).

The stub offers a frameskip (STUBCORE_SKIPPABLE), takes a set time in each
frame (STUBCORE_FRAME_US_AT: fast, then slower than a refresh for a stretch,
then fast again) and writes which frames Diatom had drawn, a 1 or a 0 each,
and when each started (STUBCORE_SHOWN_LOG).

What a slow stretch does is the same on any machine. What the fast frames
around it do is not: a machine too busy to start them on time (a CI runner
with other checks at once drew 20 frames a second) has them run late too,
and each mode then does what it does with a late frame. So the fast frames
are judged only in a run that started them on time, by the times the stub
wrote down, and otherwise the check says it could not tell.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-hotkeys does. SDL's dummy drivers: no window, no sound.

    python3 test/lateskip.py
"""
import os, re, shutil, socket, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-lateskip-")
fails, unjudged = [], []
def check(name, ok, got=None):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok: fails.append(name)

# Fast for 120 frames, 25 ms a frame (slower than a refresh) for 120, fast again
SLOW_FROM, SLOW_TO = 120, 240
SCHEDULE = f"0:2000,{SLOW_FROM}:25000,{SLOW_TO}:2000"

def run(tag, skip, frames=420, limit=90.0):
    """One game with frameskip=`skip`, until the stub has run `frames`
    frames (or `limit` seconds). The frames it was told to draw, as a string
    of 1 and 0, when each started (seconds), and Diatom's output."""
    sock, out, log = f"{TMP}/{tag}.sock", f"{TMP}/{tag}.out", f"{TMP}/{tag}.shown"
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIODRIVER="dummy", STUBCORE_SKIPPABLE="1",
               STUBCORE_FRAME_US_AT=SCHEDULE, STUBCORE_SHOWN_LOG=log)
    with open(out, "w") as f:
        p = subprocess.Popen([BIN, "--socket", sock, "--save", TMP, "--system", TMP],
                             env=env, stdout=f, stderr=subprocess.STDOUT)
    for _ in range(100):
        if os.path.exists(sock): break
        time.sleep(0.05)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.sendall(f"RUN\tcore={STUB}\tframeskip={skip}\n".encode())
    end = time.monotonic() + limit
    while time.monotonic() < end:
        time.sleep(0.25)
        try:
            with open(log) as f: n = sum(1 for _ in f)
        except OSError: n = 0
        if n >= frames: break
    s.sendall(b"STOP\n")
    time.sleep(1.5)
    s.sendall(b"QUIT\n")
    try: p.wait(5)
    except subprocess.TimeoutExpired: p.kill()
    lines = [l.split() for l in open(log)] if os.path.exists(log) else []
    lines = [l for l in lines if len(l) == 2]
    shown = "".join(l[0] for l in lines)
    starts = [int(l[1]) / 1e6 for l in lines]
    return shown, starts, open(out, errors="replace").read()

# The warmup frames run before the game's own (main.c), so frame numbers in
# the log are a few past the stub's; the stretches are judged well inside.
late, late_t, late_out = run("late", "late")
steady, steady_t, steady_out = run("steady", "steady")
slow_late = late[SLOW_FROM + 10:SLOW_TO - 5]
slow_steady = steady[SLOW_FROM + 10:SLOW_TO - 5]
check("both ran past the slow stretch", len(late) > SLOW_TO + 60 and len(steady) > SLOW_TO + 60,
      (len(late), len(steady)))
# The Mac is no real-time system: now and then a fast frame starts late, and
# each mode does what it does with a late frame (late leaves it out, steady
# draws every second one for half a second). So fast stretches are judged as
# mostly drawn, and steady by its coming back to every frame drawn.
def drawn(x):
    return x.count("1") / len(x) if x else 0.0

# Whether this run started its frames from a to b on time, by the rule
# Diatom goes by (main.c): a frame is late when it starts more than half a
# period behind the schedule. The schedule is rebuilt from the times the
# stub wrote down: a gap longer than a period puts the next frame behind by
# the difference, a shorter one takes it back (Diatom never runs ahead), and
# a stall of over four frames is dropped. One late frame in twenty is a busy
# moment; more, and this machine was too busy to tell what either mode does
# with frames that come on time.
PERIOD = 1 / 59.7275   # the stub's frame rate

def on_time(t, a, b):
    behind, late, n = 0.0, 0, 0
    for i in range(1, min(b, len(t))):
        behind = max(0.0, behind + t[i] - t[i - 1] - PERIOD)
        if behind > 4 * PERIOD: behind = 0.0
        if i >= a:
            n += 1
            late += behind > PERIOD / 2
    return n > 20 and late <= n / 20, f"{late} of {n} started late"

def fast_check(name, t, a, b, ok, got):
    timely, why = on_time(t, a, b)
    if timely: check(name, ok, got)
    else:
        print(f"  --   {name}: not judged, this machine started those frames late ({why})")
        unjudged.append(name)
    return timely

fast_check("late, fast, before the stretch: nearly every frame is drawn",
           late_t, 10, SLOW_FROM, drawn(late[10:SLOW_FROM]) >= 0.9, late[10:SLOW_FROM])
fast_check("steady, fast, before the stretch: nearly every frame is drawn",
           steady_t, 10, SLOW_FROM, drawn(steady[10:SLOW_FROM]) >= 0.7, steady[10:SLOW_FROM])
check("late, slow: frames left out, never three in a row", "0" in slow_late and "000" not in slow_late,
      slow_late)
check("late, slow: two in a row left out, so frames come unevenly", "00" in slow_late, slow_late)
check("steady, slow: exactly every second frame drawn",
      "00" not in slow_steady and "11" not in slow_steady and len(slow_steady) > 50, slow_steady)
tail = steady[SLOW_TO:]
fast_check("steady, fast again: back to every frame drawn, half a second of them in a row",
           steady_t, SLOW_TO + 35, len(steady_t), "1" * 30 in tail, steady[SLOW_TO - 5:])
check("the summary names the mode", "(frameskip=steady)" in steady_out and "(frameskip=late)" in late_out,
      [l for l in (late_out + steady_out).splitlines() if "left undrawn" in l])

shutil.rmtree(TMP, ignore_errors=True)
if fails:
    print(f"{len(fails)} failed")
    sys.exit(1)
if unjudged:
    print(f"ok: frameskip=late leaves late frames out unevenly and frameskip=steady every second one; "
          f"{len(unjudged)} check(s) of frames on time not judged, this machine started them late")
else:
    print("ok: frameskip=late leaves late frames out unevenly, frameskip=steady every second one, "
          "and both draw every frame when on time")
