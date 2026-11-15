"""How a core's life ends, end to end against the stub core:

  - A core that runs one game per load is opened afresh for each game. PrBoom
    keeps its menus and episodes in statics that its next game does not reset:
    in a resident Diatom, The Ultimate Doom after Doom II had two Quit Game
    rows, and a mod's episodes stayed in the next game's menu (1.0.5). The
    stub, named PrBoom (STUBCORE_NAME), counts the games it loaded since it
    was opened (STUBCORE_INIT_LOG): 1 at each of two games. Named otherwise
    it is kept open and counts 1, then 2.
  - A core that ends its own game (RETRO_ENVIRONMENT_SHUTDOWN: PrBoom's Quit
    Game, ScummVM's Quit) ends the session as the player quitting would: EXIT
    reason=user with no STOP sent, its state on exit written, so the shelf
    comes back. Ignored before 1.0.6, it left the last frame on the screen
    for good, which players took for a crash.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-proto does. SDL's dummy drivers: no window, no sound.

    python3 test/coreend.py
"""
import os, socket, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-coreend-")
fails = []
def check(name, ok, got=None):
    print(("  ok   " if ok else "  FAIL ") + name + ("" if ok else f"\n         got  {got}"))
    if not ok: fails.append(name)

class Run:
    def __init__(self, name, **env):
        self.sock = f"{TMP}/{name}.sock"
        self.p = subprocess.Popen(
            [BIN, "--socket", self.sock, "--save", TMP, "--system", TMP],
            env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_RENDER_DRIVER="software",
                     SDL_AUDIODRIVER="dummy", **env),
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(self.sock): break
            time.sleep(0.05)
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(self.sock)
        self.s.settimeout(0.05)
        self.buf, self.got = b"", []

    def pump(self, seconds, until=None):
        end = time.time() + seconds
        while time.time() < end:
            try:
                d = self.s.recv(65536)
                if not d: return
                self.buf += d
            except socket.timeout:
                pass
            while b"\n" in self.buf:
                l, self.buf = self.buf.split(b"\n", 1)
                self.got.append(l.decode().strip())
                if until and self.got[-1].startswith(until): return

    def send(self, line):
        self.s.sendall((line + "\n").encode())

    def close(self):
        self.send("QUIT")
        try: self.p.wait(5)
        except subprocess.TimeoutExpired: self.p.kill()

def inits(path):
    try:
        with open(path) as f: return [l.strip() for l in f]
    except OSError: return []

# One game per load: PrBoom, two games in one Diatom
for name, want in (("PrBoom", ["loads=1", "loads=1"]), ("diatom-stub", ["loads=1", "loads=2"])):
    log = f"{TMP}/{name}.loads"
    r = Run(name, STUBCORE_NAME=name, STUBCORE_INIT_LOG=log)
    r.pump(0.5, "READY")
    for _ in range(2):
        r.send(f"RUN\tcore={STUB}")
        r.pump(10, "RUNNING")
        r.pump(0.3)
        r.send("STOP")
        r.pump(10, "EXIT")
    r.close()
    if name == "PrBoom":
        check("a core named PrBoom is opened afresh for each game", inits(log) == want, inits(log))
    else:
        check("any other core stays open between games", inits(log) == want, inits(log))

# The core's own quit
state = f"{TMP}/quit.state"
r = Run("quit", STUBCORE_SHUTDOWN_AT="90")
r.pump(0.5, "READY")
r.send(f"RUN\tcore={STUB}\texit_state={state}")
r.pump(10, "RUNNING")
r.pump(15, "EXIT")
ended = [l for l in r.got if l.startswith("EXIT")]
check("a core that shuts itself down ends its game, with no STOP sent",
      ended == ["EXIT\treason=user"], r.got[-6:])
check("and its state on exit is written, as a quit's is", os.path.exists(state), os.listdir(TMP))
r.send(f"RUN\tcore={STUB}")
r.pump(10, "RUNNING")
check("and the next game runs in the same Diatom", "RUNNING" in r.got[r.got.index(ended[0]) if ended else 0:], r.got[-4:])
r.pump(0.5)
r.send("STOP")
r.pump(10, "EXIT")
r.close()

if fails:
    print(f"coreend: {len(fails)} FAILED")
    sys.exit(1)
print("ok: PrBoom opened afresh for each game; a core's own quit ends its game")
