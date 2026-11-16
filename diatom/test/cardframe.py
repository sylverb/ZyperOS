"""The frame for the launcher's card of a game with no art, end to end
against the stub core (RUN's card=, main.c's card_frame):

  - written once the game has run a minute (here DIATOM_CARD_FRAME_MS, 300
    ms), as a BMP, whole (written aside and renamed);
  - never written over: a second game with the same path leaves it as it was,
    so a card keeps its look;
  - not written by a game that ends before the minute: the launcher shows the
    Auto picture until a game has run that long.

Needs build/desktop (diatom and stubcore.so) and SKIPS without it, as
check-proto does. SDL's dummy drivers: no window, no sound.

    python3 test/cardframe.py
"""
import os, socket, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = f"{ROOT}/build/desktop/diatom"
STUB = f"{ROOT}/build/desktop/stubcore.so"
if not (os.path.exists(BIN) and os.path.exists(STUB)):
    print("skipped: no build/desktop/diatom and stubcore.so (make; make stub)")
    sys.exit(0)

TMP = tempfile.mkdtemp(prefix="diatom-cardframe-")
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

    def game(self, card, seconds):
        self.send(f"RUN\tcore={STUB}\tcard={card}")
        self.pump(10, "RUNNING")
        self.pump(seconds)
        self.send("STOP")
        self.pump(10, "EXIT")

    def close(self):
        self.send("QUIT")
        try: self.p.wait(5)
        except subprocess.TimeoutExpired: self.p.kill()

def read(path):
    try:
        with open(path, "rb") as f: return f.read()
    except OSError: return None

card = f"{TMP}/game.card.bmp"
r = Run("once", DIATOM_CARD_FRAME_MS="300")
r.pump(0.5, "READY")
r.game(card, 1.2)
first = read(card)
check("a game run past the minute leaves its card frame", first is not None, os.listdir(TMP))
check("as a whole BMP", first is not None and first[:2] == b"BM" and len(first) > 54 and
      int.from_bytes(first[2:6], "little") == len(first), first[:16] if first else None)
check("with nothing left aside", not os.path.exists(card + ".tmp"), os.listdir(TMP))
before = os.stat(card).st_mtime_ns if first else 0
r.game(card, 1.2)
check("a second game never writes over it", read(card) == first and os.stat(card).st_mtime_ns == before)
r.close()

early = f"{TMP}/early.card.bmp"
r = Run("early", DIATOM_CARD_FRAME_MS="60000")
r.pump(0.5, "READY")
r.game(early, 0.8)
r.close()
check("a game that ends before the minute writes none", not os.path.exists(early), os.listdir(TMP))

if fails:
    print(f"cardframe: {len(fails)} FAILED")
    sys.exit(1)
print("ok: the card frame is written once, past the minute, and never over itself")
