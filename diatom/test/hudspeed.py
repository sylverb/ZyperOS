"""On-Screen Stats' FPS and Speed, end to end against the stub core.

Speed is how fast the game is emulated, against its own rate: the frames it
emulated a second over the frame rate the core announces. FPS is how many
pictures the core makes a second. They are the same number for a core that
draws every frame it runs, and not for one that draws less often than that:

  - a core that runs 60 frames a second and hands over a new picture on every
    second one (a repeat, a NULL frame, on the rest) is at full speed, at
    30 fps: Speed 1.00x, FPS 30
  - a core whose retro_run is two frames long when a game shows a new picture
    every second vblank (PPSSPP, Flycast, ParallelN64 "fullspeed") is at full
    speed, at 30 fps, though retro_run was called only 30 times a second. It
    read 0.5x, the Dreamcast at 30 fps on the Brick (1.0.6). The frames it
    emulated are the sound it made: two frames of sound a call.
  - a core too slow for its game reads below 1x, and fast forward at its step

The stub is told how it draws (STUBCORE_DUPE, STUBCORE_SPAN) and how long a
call takes (STUBCORE_FRAME_US, slept); the desktop port writes down the numbers
the overlay would draw (DIATOM_HUD_LOG). And the now file px2-perf reads
(src/now.h) is held to the same speed, since it says the same thing.

How fast a CI runner runs the stub is not known here, so a reading is the
median of the last few and its bounds are wide; what is judged is the
difference between a game at full speed and one at half of it.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-fastforward does. SDL's dummy drivers: no window, no sound.

    python3 test/hudspeed.py
"""
import os, re, shutil, socket, statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-hudspeed-")
fails = []
def check(name, ok, got=None):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok: fails.append(name)

def kv(path):
    try:
        with open(path) as f:
            return dict(l.rstrip("\n").split("=", 1) for l in f if "=" in l)
    except OSError:
        return {}

def run(tag, stub_env, presses=0, seconds=4.0):
    """One game with On-Screen Stats on: `presses` times SELECT+R2 from poll 30,
    then `seconds` at that step, STOP. Returns (the FPS and the Speed the
    overlay would have said, the median of the last five readings; the speed
    against the clock from two writings of the now file; the step's own report,
    "of the clock", and whether a fast turn ran short), or None."""
    steps = ["30:select"]
    poll = 33
    for _ in range(presses):
        steps += [f"{poll}:select+r2", f"{poll + 3}:select"]
        poll += 6
    steps.append(f"{poll}:")
    sock, out, now = f"{TMP}/{tag}.sock", f"{TMP}/{tag}.out", f"{TMP}/{tag}.now"
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIODRIVER="dummy", DIATOM_INPUT_SCRIPT=";".join(steps),
               DIATOM_HUD_LOG="1", DIATOM_NOW=now, STUBCORE_PRECISE="1", **stub_env)
    with open(out, "w") as f:
        p = subprocess.Popen([BIN, "--socket", sock, "--save", TMP, "--system", TMP],
                             env=env, stdout=f, stderr=subprocess.STDOUT)
    for _ in range(100):
        if os.path.exists(sock): break
        time.sleep(0.05)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(0.1)
    buf, speeds, running = b"", [], False
    s.sendall(f"RUN\tcore={STUB}\n".encode())

    def pump(until, stop_when=None):
        nonlocal buf, running
        while time.time() < until:
            if stop_when and stop_when(): return
            try:
                d = s.recv(65536)
                if not d: return
                buf += d
            except socket.timeout:
                pass
            while b"\n" in buf:
                l, buf = buf.split(b"\n", 1)
                l = l.decode().strip()
                if l == "RUNNING" and not running:
                    running = True
                    s.sendall(b"SETHUD\ton=1\n")
                if l.startswith("SPEED\tx="): speeds.append(l.split("=", 1)[1])
                if l == "PAUSED": s.sendall(b"RESUME\n")

    pump(time.time() + 10, lambda: running)
    end = time.time() + 30
    while len(speeds) < presses and time.time() < end:
        pump(time.time() + 0.2)
    # The step is in force: the now file asked for, read twice, a second apart
    # in the middle of the time held
    pump(time.time() + seconds / 2)
    open(now + ".ask", "w").close()
    pump(time.time() + 0.6)
    a = kv(now)
    pump(time.time() + max(1.0, seconds / 2 - 0.6))
    b = kv(now)
    s.sendall(b"STOP\n")
    pump(time.time() + 1.5)
    s.sendall(b"QUIT\n")
    try: p.wait(5)
    except subprocess.TimeoutExpired: p.kill()
    text = open(out, errors="replace").read()
    huds = [(float(f), float(v)) for f, v in
            re.findall(r"diatom: hud fps=([\d.]+) speed=(-?[\d.]+)", text)]
    if len(huds) < 3: return None
    last = huds[-5:]
    fps = statistics.median(h[0] for h in last)
    speed = statistics.median(h[1] for h in last)
    nspeed = None
    if a.get("seq") and b.get("seq") and a["seq"] != b["seq"]:
        df = float(b["frames"]) - float(a["frames"])
        dr = float(b["run_us"]) - float(a["run_us"])
        if dr > 0 and float(b["fps"]) > 0:
            nspeed = df * 1e6 / float(b["fps"]) / dr
    rep = re.findall(r"diatom: speed ([\d.]+)x held [\d.]+s, reached [\d.]+x "
                     r"\(([\d.]+)x of the clock\), \d+ audio frame\(s\) dropped, "
                     r"slept [\d.]+s \([\d.]+s after (\d+) of \d+ turns short", text)
    clock, short = (float(rep[-1][1]), int(rep[-1][2])) if rep else (None, None)
    return fps, speed, nspeed, clock, short

def near(x, lo, hi):
    return x is not None and lo <= x <= hi

def say(name, r):
    if r: print(f"       {name}: FPS {r[0]:.1f}, Speed {r[1]:.2f}x"
                + (f", the now file {r[2]:.2f}x" if r[2] else ""))

CORE_FPS = 59.7275
PERIOD_US = 1e6 / CORE_FPS

def stands(r, frames):
    """Speed is the pictures a second times the frames each stands for, over
    the core's rate, however busy this machine is: a core that is two frames
    to a picture and draws 26.6 a second is at 0.89x, and the count of calls
    (0.44x) is what it was before. Both are read from the same run, so a
    machine too busy to keep a core's speed does not move this."""
    return r is not None and abs(r[1] - r[0] * frames / CORE_FPS) <= 0.03 + 0.04 * r[1]

def same_now(r):
    """The now file's speed against the clock, from two writings, the same"""
    return r is not None and r[2] is not None and abs(r[2] - r[1]) <= 0.06 + 0.06 * r[1]

# The control: a core that draws every frame it runs, at full speed
plain = run("plain", {}, seconds=5)
say("plain", plain)
check("a core that draws every frame is at 60 fps and 1.00x",
      plain and near(plain[0], 50, 62) and near(plain[1], 0.5, 1.06) and stands(plain, 1), plain)
# On a machine too busy to run even the stub at its speed, what follows is
# judged by what a run says of itself (stands) and never over 1x; where it
# kept up, by the speed itself too
kept_up = plain is not None and plain[1] >= 0.93
if not kept_up:
    print("  --   absolute speeds not judged: this machine could not keep the control at its speed")

def absolute(name, r, lo, hi, fps_hi):
    check(name + ": never over its speed or its fps", r and r[1] <= hi and r[0] <= fps_hi, r)
    if kept_up:
        check(name + ": and near it", r and r[1] >= lo, r)

# 60 frames a second, a new picture every second one: full speed, 30 fps
r = run("dupe", {"STUBCORE_DUPE": "2"}, seconds=5)
say("dupe", r)
check("a core that repeats every second picture draws 30 fps, not 60, and each is two frames",
      r and r[0] <= 33 and stands(r, 2), r)
absolute("and it is at 1.00x", r, 0.9, 1.06, 33)
check("and the now file says the same speed", same_now(r), r)

# A call two frames long, a picture and two frames of sound, taking two
# frames' time: full speed at 30 fps, called 30 times a second
r = run("span", {"STUBCORE_SPAN": "2", "STUBCORE_FRAME_US": str(int(2 * PERIOD_US) + 100)}, seconds=5)
say("span", r)
check("a core whose call is two frames long is two frames to a picture, not 0.5x",
      r and r[0] <= 31.5 and stands(r, 2), r)
absolute("and it is at 1.00x", r, 0.85, 1.06, 31.5)
check("and the now file says the same speed", same_now(r), r)

# The same, three frames long: a 20 fps game
r = run("span3", {"STUBCORE_SPAN": "3", "STUBCORE_FRAME_US": str(int(3 * PERIOD_US) + 100)}, seconds=5)
say("span3", r)
check("a core whose call is three frames long is three frames to a picture, not 0.33x",
      r and r[0] <= 21 and stands(r, 3), r)
absolute("and it is at 1.00x", r, 0.85, 1.06, 21)

# A core too slow for its game reads under 1x, and so does a 30 fps game
# that is slow: here a call of 3 frames' time that is 2 frames long
r = run("slow", {"STUBCORE_FRAME_US": "25000"}, seconds=5)
say("slow", r)
check("a core at 25 ms a frame is at about 0.67x, one frame to a picture",
      r and r[1] <= 0.72 and r[1] >= 0.3 and r[0] <= 41 and stands(r, 1), r)
r = run("slowspan", {"STUBCORE_SPAN": "2", "STUBCORE_FRAME_US": str(int(3 * PERIOD_US))}, seconds=5)
say("slowspan", r)
check("a 30 fps game at two thirds of its speed is at about 0.67x and 20 fps",
      r and r[1] <= 0.72 and r[1] >= 0.3 and r[0] <= 21 and stands(r, 2), r)
check("and the now file says the same speed", same_now(r), r)

# Fast forward: the step, dupes or not. A 1 ms core reaches it on a machine
# that keeps up; on a busy one the overlay says what the step's own report
# says it reached, near enough, and never over the step
for presses, want in ((2, 2.0), (3, 3.0)):
    for tag, stub_env in (("", {}), ("dupe", {"STUBCORE_DUPE": "2"})):
        r = run(f"ff{presses}{tag}", dict(stub_env, STUBCORE_FRAME_US="1000"), presses, seconds=5)
        say(f"{want}x {tag or 'plain'}", r)
        short = r is not None and r[4] is not None and r[4] > 0
        tol = 0.45 if short else 0.15
        check(f"fast forward at {want}x, {tag or 'every frame drawn'}: the overlay says what the step reached",
              r and r[3] is not None and near(r[1], r[3] - tol, r[3] + tol) and 1.0 < r[1] <= want * 1.06, r)
        if r and not short:
            check(f"and with no turn short of the step, near {want}x", r[1] >= want * 0.88, r)
        elif r:
            print(f"  --   not judged against {want}x: this machine could not keep the step")

shutil.rmtree(TMP, ignore_errors=True)
if fails:
    print(f"{len(fails)} failed")
    sys.exit(1)
print("ok: Speed is the game's speed against its own rate, FPS the pictures a second, dupes or long calls or not")
