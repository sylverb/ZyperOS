"""Fast forward's speed, end to end against the stub core.

A fast turn stands for two refreshes. Until 2026-11-16 it was also paced as
two whatever ran in it, so a core whose frames took more than a third of the
turn ran 2 frames in it and then slept: Pokemon Black on the Pixel 2, at
11 to 12 ms a frame, went 1.0x at every step past 1.5x, with the emulation
thread idle a fifth of the time. A turn is now paced as the time its frames
stand for at the step chosen, so a core that cannot reach the step runs
flat out, and one that can is held to it, never over.

The stub takes STUBCORE_FRAME_US in each frame, as a slow core does, and
with STUBCORE_SKIPPABLE offers a frameskip, as the DS core does. The speed is
Diatom's own figure at STOP (step_report), "of the clock": frames run, as the
game's time, over the wall clock's.

Sound is not judged here: SDL's dummy driver drains its queue slower than
the clock, and drops at 1x too. The silence fast forward pushes (one turn's
wall-clock time) is checked on a device, where step_report's count is real.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-hotkeys does. SDL's dummy drivers: no window, no sound.

    python3 test/fastforward.py
"""
import os, re, shutil, socket, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-ff-")
fails = []
def check(name, ok, got=None):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok: fails.append(name)

def run(tag, frame_us, presses, seconds=4.0, skippable=False):
    """One game: SELECT+R2 `presses` times from poll 30, then `seconds` at that
    step, STOP. Returns (step, of the clock, audio frames dropped) from the
    step's report, or None."""
    steps = ["30:select"]
    poll = 33
    for _ in range(presses):
        steps += [f"{poll}:select+r2", f"{poll + 3}:select"]
        poll += 6
    steps.append(f"{poll}:")
    sock, out = f"{TMP}/{tag}.sock", f"{TMP}/{tag}.out"
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software",
               SDL_AUDIODRIVER="dummy", DIATOM_INPUT_SCRIPT=";".join(steps),
               STUBCORE_FRAME_US=str(frame_us))
    if skippable: env["STUBCORE_SKIPPABLE"] = "1"
    with open(out, "w") as f:
        p = subprocess.Popen([BIN, "--socket", sock, "--save", TMP, "--system", TMP],
                             env=env, stdout=f, stderr=subprocess.STDOUT)
    for _ in range(100):
        if os.path.exists(sock): break
        time.sleep(0.05)
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.settimeout(0.1)
    buf, speeds = b"", []
    s.sendall(f"RUN\tcore={STUB}\n".encode())

    def pump(until):
        nonlocal buf
        while time.time() < until:
            try:
                d = s.recv(65536)
                if not d: return
                buf += d
            except socket.timeout:
                pass
            while b"\n" in buf:
                l, buf = buf.split(b"\n", 1)
                l = l.decode().strip()
                if l.startswith("SPEED\tx="): speeds.append(l.split("=", 1)[1])
                if l == "PAUSED": s.sendall(b"RESUME\n")

    # Until the last step is said, then the time at it
    end = time.time() + 30
    while len(speeds) < presses and time.time() < end:
        pump(time.time() + 0.2)
    pump(time.time() + seconds)
    s.sendall(b"STOP\n")
    pump(time.time() + 1.5)
    s.sendall(b"QUIT\n")
    try: p.wait(5)
    except subprocess.TimeoutExpired: p.kill()
    text = open(out, errors="replace").read()
    reps = re.findall(r"diatom: speed ([\d.]+)x held [\d.]+s, reached [\d.]+x "
                      r"\(([\d.]+)x of the clock\), (\d+) audio frame", text)
    # What a frame really cost in this run, drawn or not: on a busy machine
    # (make check runs many at once) more than the stub spends
    costs = [float(c) for c in re.findall(r"diatom: the core took ([\d.]+) ms a drawn frame", text)]
    costs += [float(c) for c in re.findall(r"diatom: and ([\d.]+) ms a frame run undrawn", text)]
    if not reps or not costs: return None
    step, clock, dropped = reps[-1]
    return float(step), float(clock), int(dropped), sum(costs) / len(costs)

# A core that reaches every step: held to it, never over
for presses, want in ((1, 1.5), (3, 3.0)):
    r = run(f"fast{presses}", 1000, presses)
    check(f"a 1 ms core at {want}x runs at {want}x, not over it",
          r is not None and r[0] == want and want * 0.92 <= r[1] <= want * 1.03, r)

# A slow core, flat out short of the step. The old pacing ran whole frames in
# a two-refresh turn and slept the rest, so its speed came in steps: at 9 ms
# a frame 3 a turn, 1.5x; a little over 10.5 ms, 2 a turn, 1.0x (Pokemon
# Black on the Pixel 2, 11 to 12 ms, went 1.0x). Flat out is what the frame
# cost allows, 1.86x at 9 ms; judged against the cost this run measured, since
# a sleep runs over on a busy machine.
def flat_of(r):
    return 1000.0 / r[3] / 59.7275

for tag, skippable in (("slow", True), ("slow-noskip", False)):
    r = run(tag, 9000, 3, skippable=skippable)
    say = "with a frameskip" if skippable else "with no frameskip"
    check(f"a 9 ms core {say} at 3x runs flat out, not whole frames a turn",
          r is not None and r[0] == 3.0 and flat_of(r) * 0.88 <= r[1] <= flat_of(r) * 1.05,
          r and (r, round(flat_of(r), 2)))
    if r: print(f"       {r[1]:.2f}x, flat out {flat_of(r):.2f}x at {r[3]:.2f} ms a frame")

# At a step it can reach it is held to the step; on a machine busy enough
# that it cannot, flat out
r = run("slow15", 9000, 1, skippable=True)
check("the 9 ms core at 1.5x runs at 1.5x (or flat out short of it), not over it",
      r is not None and r[0] == 1.5 and
      min(1.5, flat_of(r)) * 0.88 <= r[1] <= 1.5 * 1.03,
      r and (r, round(flat_of(r), 2)))

shutil.rmtree(TMP, ignore_errors=True)
if fails:
    print(f"{len(fails)} failed")
    sys.exit(1)
print("ok: fast forward runs a slow core flat out, and a fast one at its step, never over it")
