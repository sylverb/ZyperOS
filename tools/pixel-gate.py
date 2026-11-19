#!/usr/bin/env python3
"""The same pixels, proved: every shot the launcher takes, at a base and here.

    make pixel-gate                         against origin/main, both themes,
                                            every language
    python3 tools/pixel-gate.py --base REF  against REF
    python3 tools/pixel-gate.py --lang en --theme light menu usb-ask
                                            one language, one theme, some shots
    python3 tools/pixel-gate.py --device brick
                                            as the Brick (the Pixel 2 by default,
                                            the README's handheld)
    python3 tools/pixel-gate.py --targeted  only what this change can touch
                                            (below): a pull request's rounds
    python3 tools/pixel-gate.py --warm      the base's frames into the shared
                                            cache and nothing else, as after a
                                            merge to main (make pixel-gate-warm)

A change that moves code and means to change nothing on the screen (a part
moved into src/ds/, a number that became a token) is held to it: the
launcher is built at the base in a worktree of its own and here, and both
take every shot of tools/i18n-shots.py (the README's screens and the rest:
check-i18n-shots's list) in each language and theme asked for, from the same
scratch cards, made from this tree's res/. The frames are compared as decoded
pixels, not as PNG bytes. It prints "pixel gate: K of N frames changed", lists
each one, and for the changed ones writes sheets of the base, this tree and the
pixels that differ (build-native/pixel-gate/sheet-*.png): the before and after
of a change that means to change something, for a review.

What changed is shot twice again on each side, a second look: a frame that
differs from its own side's first shot shows something that moves by itself
(the time, a clock-driven Zyper's blink, the host's free disk space), and is
listed as unsteady, not as changed. The looks can agree by chance (one frame
of 3,763 on 2026-11-18: 12 pixels of Zyper), so a changed frame of a screen
that is unsteady elsewhere in the run says so in the report.

The base's frames are shared by every worktree on this computer, frame by
frame. They are a function of the base's commit (its launcher), of what this
tree hands the shots (res/ for the cards but its strings, which are compiled
in; tools/fixtures; config/systems.cfg; the shot tools and their list of
shots) and of the host (its compiler, SDL's versions): kept under that key in
one folder (ZYPEROS_GATE_CACHE; ~/Library/Caches/ZyperOS/pixel-gate on a Mac,
~/.cache/zyperos/pixel-gate elsewhere), so a run shoots only the base frames
no run has shot yet, under a lock that makes a second run at the same base
wait and take the first's. The base's launcher is kept beside them by its
commit. Shot once a main commit (--warm), every pull request that has merged
that main and leaves res/ and the shot tools alone shoots only its own side.
The folder keeps the KEEP (4, ZYPEROS_GATE_KEEP) bases used last and none
unused for two days. This tree's frames are deleted when the run ends
(--keep-frames keeps them in build-native/pixel-gate/here); the report and the
sheets stay.

--targeted, for a pull request's rounds; the full gate is the release
candidate's:
  1. When nothing the launcher is built from changed against the base (src/,
     res/strings/, tools/i18n.py, mk/native.mk, the Makefile), no frame can,
     and nothing is built or shot.
  2. Otherwise a first look: every shot in English, the pseudo-locale (the
     longest words), German (the longest real ones: where a line breaks moves
     with them, and the pseudo-locale's padding does not break where they do)
     and Chinese (the CJK face) in light, English in dark, and every language
     whose strings changed or that the change's C names (LANG_JA ...), in
     light.
  3. Every screen that changed anywhere in that first look is shot in every
     language and theme asked, on both sides, and compared as the full gate
     does.
What a change alters only in a language and theme the first look leaves out,
with no string of that language changed, is missed: the full gate before a
release catches it. (Measured 2026-11-18: one note line's height in the delete
dialog changed how many lines fit on Storage's States screen in German and
Spanish only; without German in the first look, those 4 of 54 frames were
missed.)

Exit 0 when no frame changed, 1 when one did (or with --allow-changes, 0 and
the list), 2 when a build or a side's shots failed to run at all.

Not in `make check`: it builds the launcher twice and takes about 13,000
frames a side (721 shots, 9 languages, 2 themes). CPU: --jobs (3) launchers
at once.
"""
import argparse
import fcntl
import hashlib
import importlib.util
import os
from pathlib import Path
import platform
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build-native" / "pixel-gate"
SHEET_ROWS = 10
# Bases kept, and how long one unused is kept
KEEP = int(os.environ.get("ZYPEROS_GATE_KEEP") or 4)
KEEP_DAYS = 2

# What this tree hands the base's shots besides the launcher: the cards'
# files and the tools that make the cards and take the shots. res/strings is
# compiled into each side's launcher, and res/web/i18n is made from it and
# drawn by no launcher shot: neither is a part of the key.
INPUTS = ["res", "tools/i18n-shots.py", "tools/readme-shots.py", "tools/fixtures", "config/systems.cfg"]
NOT_INPUTS = {"res/strings", "res/web/i18n"}

# What the host launcher is built from: a change to none of them changes no
# frame (--targeted). The Makefile's part is its VERSION, which About draws
# (version_changed): a check added there builds nothing the shots run.
BUILT_FROM = ("src/", "res/strings/", "tools/i18n.py", "mk/native.mk")

# The first look of --targeted: (theme, language) pairs
PROBE = [("light", "en"), ("light", "xx"), ("light", "de"), ("light", "zh-Hans"), ("dark", "en")]


def run(cmd, cwd, **kw):
    return subprocess.run(cmd, cwd=cwd, text=True, **kw)


def capture(cmd, cwd=ROOT, env=None):
    try:
        r = subprocess.run(cmd, cwd=cwd, env=env, text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL)
    except OSError:
        return ""
    return r.stdout.strip() if r.returncode == 0 else ""


def rev(ref):
    r = run(["git", "rev-parse", "--verify", ref + "^{commit}"], ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL)
    if r.returncode:
        sys.exit("pixel gate: no commit %s" % ref)
    return r.stdout.strip()


def cache_root():
    if os.environ.get("ZYPEROS_GATE_CACHE"):
        return Path(os.environ["ZYPEROS_GATE_CACHE"]).expanduser()
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Caches" / "ZyperOS" / "pixel-gate"
    return Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache") / "zyperos" / "pixel-gate"


def host():
    """What the host draws with: its system, its compiler, SDL's libraries and
    the faces' rasterizer under them"""
    cc = os.environ.get("CC") or "cc"
    libs = [capture(["pkg-config", "--modversion", lib]) for lib in
            ("sdl2", "SDL2_image", "SDL2_ttf", "freetype2", "harfbuzz")]
    return "\n".join([platform.system(), platform.machine(),
                      (capture([cc, "--version"]).splitlines() or [""])[0], " ".join(libs)])


def inputs():
    """The git hashes of INPUTS as they are in this tree, committed or not:
    staged into a copy of this worktree's index (whose file times spare
    hashing what has not changed), written as a tree, read back"""
    paths = [p for p in INPUTS if (ROOT / p).exists()]
    with tempfile.TemporaryDirectory(prefix="zyperos-gate-index-") as tmp:
        index = Path(tmp) / "index"
        real = capture(["git", "rev-parse", "--path-format=absolute", "--git-path", "index"])
        if real and Path(real).is_file():
            shutil.copyfile(real, index)
        env = dict(os.environ, GIT_INDEX_FILE=str(index))
        if run(["git", "add", "-A", "--", *paths], ROOT, env=env, stdout=subprocess.DEVNULL,
               stderr=subprocess.DEVNULL).returncode:
            sys.exit("pixel gate: git add failed on %s" % " ".join(paths))
        tree = capture(["git", "write-tree"], env=env)
        if not tree:
            sys.exit("pixel gate: git write-tree failed")
        top = capture(["git", "ls-tree", tree, "--", *paths]).splitlines()
        res = capture(["git", "ls-tree", tree + ":res"]).splitlines()
        web = capture(["git", "ls-tree", tree + ":res/web"]).splitlines()
    lines = [l for l in top if not l.endswith("\tres")]
    lines += ["res/" + l for l in res if "res/" + l.split("\t")[-1] not in NOT_INPUTS
              and not l.endswith("\tweb")]
    lines += ["res/web/" + l for l in web if "res/web/" + l.split("\t")[-1] not in NOT_INPUTS]
    return "\n".join(lines)


def shots_module():
    spec = importlib.util.spec_from_file_location("i18n_shots", ROOT / "tools" / "i18n-shots.py")
    mod = importlib.util.module_from_spec(spec)
    sys.path.insert(0, str(ROOT / "tools"))
    spec.loader.exec_module(mod)
    return mod


class Lock:
    """An exclusive lock on a file, held while a base is shot or a launcher
    built, so a second run at the same base waits and takes what the first
    made rather than making it again"""
    def __init__(self, path, what):
        self.path, self.what = path, what

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.f = open(self.path, "a+")
        try:
            fcntl.flock(self.f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            self.f.seek(0)
            print("pixel gate: waiting for another run %s (%s)" % (self.what, self.f.read().strip() or "?"),
                  flush=True)
            fcntl.flock(self.f, fcntl.LOCK_EX)
        self.f.seek(0)
        self.f.truncate()
        self.f.write("pid %d, %s\n" % (os.getpid(), ROOT))
        self.f.flush()
        return self

    def __exit__(self, *exc):
        fcntl.flock(self.f, fcntl.LOCK_UN)
        self.f.close()


def build(tree, jobs):
    """The host launcher in `tree`, as make native builds it"""
    r = run(["make", "-s", "-j%d" % jobs, "native"], tree, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, errors="replace")
    binary = tree / "build-native" / "zyperos"
    if r.returncode or not binary.is_file():
        print(r.stdout[-3000:], file=sys.stderr)
        print("pixel gate: the launcher did not build in %s" % tree, file=sys.stderr)
        sys.exit(2)
    return binary


def base_binary(cache, sha, hostkey, jobs):
    """The base's launcher, built once a commit and host in a worktree of its
    own, and kept"""
    folder = cache / ("bin-%s-%s" % (sha[:12], hostkey[:10]))
    binary = folder / "zyperos"
    in_use(folder)
    with Lock(folder / "lock", "building the base's launcher"):
        if binary.is_file():
            return binary
        with tempfile.TemporaryDirectory(prefix="zyperos-pixel-gate-") as tmp:
            tree = Path(tmp) / "base"
            run(["git", "worktree", "add", "--detach", "-q", str(tree), sha], ROOT, check=True)
            try:
                print("pixel gate: building the base, %s" % sha[:12], flush=True)
                built = build(tree, jobs)
                shutil.copy2(built, folder / "zyperos.new")
                os.replace(folder / "zyperos.new", binary)
            finally:
                run(["git", "worktree", "remove", "--force", str(tree)], ROOT)
    return binary


def shoot(binary, out, langs, theme, only, jobs, device):
    """Shots into out/<lang>/<name>.png, with this tree's i18n-shots (its
    list, its cards), whatever the binary is: the output, and whether a
    frame came"""
    cmd = [sys.executable, str(ROOT / "tools" / "i18n-shots.py"), "--binary", str(binary),
           "--theme", theme, "--jobs", str(jobs), "--keep", str(out), "--device", device]
    if CARDS:
        cmd += ["--cards", CARDS[0]]
    for code in langs:
        cmd += ["--lang", code]
    cmd += only
    # Its own verdict (what does not fit) is check-i18n-shots's business: here
    # only the frames count. Stopped, it is asked to stop too, and given the
    # time to delete its scratch cards
    p = subprocess.Popen(cmd, cwd=ROOT, text=True, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, errors="replace")
    CHILDREN.add(p)
    try:
        output, _ = p.communicate()
    finally:
        CHILDREN.discard(p)
    return output, any(out.glob("*/*.png"))


# The scratch cards every run of i18n-shots in this gate clones its own from,
# made once (cards()) from this tree's res/, as both sides' are
CARDS = []


def cards(mod):
    if not CARDS:
        top = Path(tempfile.mkdtemp(prefix="zyperos-gate-cards-"))
        CARDS.append(str(top))
        mod.make_cards(top)
    return CARDS[0]


# The runs of i18n-shots in flight, so that a stop stops them: asked to end
# (SIGTERM), each deletes its scratch cards; one that has not in 30 s is killed
CHILDREN = set()
STOPPING = []


def stop_children():
    STOPPING.append(True)
    kids = list(CHILDREN)
    for p in kids:
        p.terminate()
    for p in kids:
        try:
            p.wait(timeout=30)
        except subprocess.TimeoutExpired:
            p.kill()


def frames(top):
    return {p.relative_to(top).as_posix(): p for p in top.glob("*/*/*.png")}


def wanted(pairs, names):
    """theme/lang/name.png, as frames() names them, for each (theme, lang)
    pair and shot name"""
    return {"%s/%s/%s.png" % (t, l, n) for t, l in pairs for n in names}


def shot_of(key):
    """The shot's name in a frame's theme/lang/name.png"""
    return key.split("/", 2)[2][:-len(".png")]


def fill(binary, top, want, all_names, jobs, device):
    """Shoots into top/ each frame of `want` that it lacks, `jobs` launchers
    at a time: a theme's languages that lack the same shots in one run of
    i18n-shots (one a language at once), split by shots where there are
    fewer languages than jobs; the frames moved in whole, so a reader at the
    same time sees a frame or none"""
    import concurrent.futures
    import threading
    missing = sorted(set(want) - set(frames(top)))
    if not missing:
        return 0
    groups = {}
    for key in missing:
        theme, lang, _ = key.split("/", 2)
        groups.setdefault((theme, lang), set()).add(shot_of(key))
    calls = {}
    for (theme, lang), names in groups.items():
        calls.setdefault((theme, frozenset(names)), []).append(lang)
    units = []
    for (theme, names), langs in sorted(calls.items(), key=lambda c: (c[0][0], sorted(c[1]))):
        langs = sorted(langs)
        ordered = [n for n in all_names if n in names] + sorted(names - set(all_names))
        parts = max(1, min(len(ordered), jobs // len(langs)))
        for i in range(parts):
            chunk = ordered[i::parts]
            only = [] if parts == 1 and names >= set(all_names) else chunk
            units.append((theme, langs, only, min(len(langs), jobs), len(chunk)))
    # Launchers in flight, at most jobs (one unit always runs)
    budget = threading.Condition()
    busy = [0]
    tmp_root = top.parent / ("shoot-%d" % os.getpid())

    def go(i, unit):
        theme, langs, only, n, count = unit
        with budget:
            budget.wait_for(lambda: busy[0] == 0 or busy[0] + n <= jobs)
            busy[0] += n
        try:
            tmp = tmp_root / str(i)
            output, got = shoot(binary, tmp, langs, theme, only, n, device)
            # A broken launcher draws nothing at all; a few shots this side
            # has not (a screen new here) draw nothing either, and are fine
            if STOPPING:
                return True
            if not got and count * 2 > len(all_names):
                print(output[-3000:], file=sys.stderr)
                print("pixel gate: no frames from %s in %s" % (binary, theme), file=sys.stderr)
                return False
            for path in tmp.glob("*/*.png"):
                dest = top / theme / path.parent.name / path.name
                dest.parent.mkdir(parents=True, exist_ok=True)
                os.replace(path, dest)
            return True
        finally:
            with budget:
                busy[0] -= n
                budget.notify_all()

    pool = concurrent.futures.ThreadPoolExecutor(max_workers=max(1, jobs))
    try:
        futures = [pool.submit(go, i, u) for i, u in enumerate(units)]
        ok = [f.result() for f in futures]
        pool.shutdown()
    except BaseException:
        stop_children()
        pool.shutdown(wait=True, cancel_futures=True)
        raise
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)
    if not all(ok):
        sys.exit(2)
    return len(missing)


def decode(path):
    from PIL import Image
    with Image.open(path) as im:
        im = im.convert("RGBA")
        return im.size, im.tobytes()


def same(a, b):
    if a.read_bytes() == b.read_bytes():
        return True
    try:
        return decode(a) == decode(b)
    except ImportError:
        return False


def sheets(changed, base, here, out):
    """Rows of base | here | the differing pixels (magenta over the base,
    dimmed), half size, SHEET_ROWS a sheet"""
    try:
        from PIL import Image, ImageChops, ImageDraw, ImageFont
    except ImportError:
        print("pixel gate: no Pillow, so no sheets")
        return []
    try:
        font = ImageFont.load_default(size=20)   # Pillow 10.1+
    except TypeError:
        font = ImageFont.load_default()
    made = []
    for start in range(0, len(changed), SHEET_ROWS):
        part = changed[start:start + SHEET_ROWS]
        w, h, label = 512, 384, 28
        sheet = Image.new("RGB", (3 * w + 4 * 8, len(part) * (h + label + 8) + 8), (40, 42, 40))
        draw = ImageDraw.Draw(sheet)
        for i, name in enumerate(part):
            a = Image.open(base[name]).convert("RGB")
            b = Image.open(here[name]).convert("RGB")
            if a.size != b.size:
                b = b.resize(a.size)
            diff = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v else 0)
            mark = Image.blend(a, Image.new("RGB", a.size, (0, 0, 0)), 0.6)
            mark.paste(Image.new("RGB", a.size, (255, 0, 200)), mask=diff)
            y = 8 + i * (h + label + 8)
            draw.text((8, y + 6), "%s   (before | after | changed pixels: %d)" % (
                name, sum(1 for v in diff.getdata() if v)), fill=(228, 226, 216), font=font)
            for k, im in enumerate((a, b, mark)):
                sheet.paste(im.resize((w, h)), (8 + k * (w + 8), y + label))
        path = out / ("sheet-%02d.png" % (start // SHEET_ROWS + 1))
        sheet.save(path)
        made.append(path)
    return made


def prune(cache, keep):
    """The KEEP bases (and launchers) used last, and none unused for
    KEEP_DAYS; one another run holds is left alone"""
    now = time.time()
    for kind in ("base-", "bin-"):
        folders = []
        for f in cache.glob(kind + "*"):
            used = f / "used"
            folders.append((used.stat().st_mtime if used.exists() else 0, f))
        folders.sort(reverse=True)
        for i, (when, f) in enumerate(folders):
            if f in keep or (i < KEEP and now - when < KEEP_DAYS * 86400):
                continue
            # A run that reads it holds "inuse" shared, one that shoots it
            # holds "lock": either leaves it be
            try:
                with open(f / "inuse", "a+") as a, open(f / "lock", "a+") as b:
                    fcntl.flock(a, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    fcntl.flock(b, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    shutil.rmtree(f, ignore_errors=True)
            except (BlockingIOError, FileNotFoundError):
                pass


def in_use(folder):
    """A shared hold on a kept folder for as long as this run lives, so that
    no other run's prune deletes it under this one"""
    folder.mkdir(parents=True, exist_ok=True)
    f = open(folder / "inuse", "a+")
    fcntl.flock(f, fcntl.LOCK_SH)
    (folder / "used").touch()
    HELD.append(f)
    return f


HELD = []


def changed_files(sha):
    """What differs from the base in this tree: commits, the working tree
    and files git does not track yet"""
    files = set(capture(["git", "diff", "--name-only", sha]).splitlines())
    files |= set(capture(["git", "ls-files", "--others", "--exclude-standard"]).splitlines())
    return sorted(f for f in files if f)


def version_changed(sha):
    """Whether the Makefile's VERSION, which the launcher is built with,
    differs from the base's"""
    def version(text):
        return [l for l in text.splitlines() if l.startswith("VERSION ?=")]
    base = subprocess.run(["git", "show", sha + ":Makefile"], cwd=ROOT, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.DEVNULL).stdout
    return version(base) != version((ROOT / "Makefile").read_text(encoding="utf-8"))


def string_langs(sha, i18n):
    """The languages whose strings differ between the base and this tree
    (the pseudo-locale with English, which it is made from)"""
    def texts():
        got = {}
        for s in i18n.parse([]):
            for code, text in s.text.items():
                got[(s.id, code)] = repr(text)
        return got
    if not any(f.startswith("res/strings/") for f in changed_files(sha)):
        return set()
    here = texts()
    import io
    import tarfile
    with tempfile.TemporaryDirectory(prefix="zyperos-gate-strings-") as tmp:
        tar = subprocess.run(["git", "archive", "--format=tar", sha, "res/strings"], cwd=ROOT,
                             stdout=subprocess.PIPE, check=True).stdout
        with tarfile.open(fileobj=io.BytesIO(tar)) as t:
            for m in t.getmembers():
                if m.isfile() and m.name.endswith(".txt"):
                    Path(tmp, os.path.basename(m.name)).write_bytes(t.extractfile(m).read())
        kept = i18n.SRC_DIR
        i18n.SRC_DIR = tmp
        try:
            base = texts()
        finally:
            i18n.SRC_DIR = kept
    langs = {code for _, code in set(here) ^ set(base)}
    langs |= {code for k in set(here) & set(base) if here[k] != base[k] for code in [k[1]]}
    if "en" in langs:
        langs.add(i18n.PSEUDO)
    return langs


def named_langs(sha, i18n):
    """The languages the change's C names by their enum (LANG_JA)"""
    import re
    diff = capture(["git", "diff", "-U0", sha, "--", "src/"])
    enum = {l[2]: l[0] for l in i18n.LANGS}
    found = set()
    for line in diff.splitlines():
        if line.startswith(("+", "-")) and not line.startswith(("+++", "---")):
            for name in re.findall(r"\bLANG_[A-Z_]+\b", line):
                if name in enum:
                    found.add(enum[name])
    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", default="origin/main", help="the commit to compare with (origin/main)")
    ap.add_argument("--lang", action="append", help="this language only (more than once for more)")
    ap.add_argument("--theme", action="append", choices=["light", "dark"], help="this theme only")
    ap.add_argument("--jobs", type=int, default=3, help="launchers at once (3)")
    ap.add_argument("--device", default="pixel2", help="the handheld the shots are taken as (pixel2)")
    ap.add_argument("--allow-changes", action="store_true", help="exit 0 with changes: a change meant")
    ap.add_argument("--fresh", action="store_true", help="shoot the base again even if kept")
    ap.add_argument("--targeted", action="store_true",
                    help="a first look, then only the screens it found changed (see above)")
    ap.add_argument("--warm", action="store_true", help="the base's frames into the cache, nothing else")
    ap.add_argument("--keep-frames", action="store_true", help="keep this tree's frames when done")
    ap.add_argument("only", nargs="*", help="shot names (default: all)")
    args = ap.parse_args()

    # A kill or a closed terminal ends the run as Ctrl-C does: through the
    # finally blocks, which take this run's frames and scratch with them
    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, stop)

    # Frames are compared as pixels: two PNGs of the same pixels can differ
    # in their bytes, and without Pillow every such frame looked changed
    try:
        import PIL  # noqa: F401
    except ImportError:
        sys.exit("pixel gate: needs Pillow to compare pixels: make pixel-gate runs it with a "
                 "Python that has it (PYIMG)")
    sys.path.insert(0, str(ROOT / "tools"))
    import i18n
    mod = shots_module()
    all_names = list(mod.shot_list())
    names = [n for n in all_names if not args.only or n in args.only]
    if args.only and not names:
        sys.exit("pixel gate: no such shot: %s" % " ".join(args.only))
    langs = args.lang or (i18n.CODES + [i18n.PSEUDO])
    themes = args.theme or ["light", "dark"]
    pairs = [(t, l) for t in themes for l in langs]
    sha = rev(args.base)
    OUT.mkdir(parents=True, exist_ok=True)
    # The bases each worktree kept before they were shared
    for old in OUT.glob("base-*"):
        shutil.rmtree(old, ignore_errors=True)
    here_out = OUT / "here"
    for old in [here_out, *OUT.glob("again-*"), *OUT.glob("sheet-*.png")]:
        if old.is_dir():
            shutil.rmtree(old, ignore_errors=True)
        elif old.exists():
            old.unlink()

    note = []
    if args.targeted:
        changed = changed_files(sha)
        built = [f for f in changed if f.startswith(BUILT_FROM)]
        if "Makefile" in changed and version_changed(sha):
            built.append("Makefile")
        if not built:
            report = ["pixel gate: 0 of 0 frames changed (base %s, targeted): nothing the launcher is "
                      "built from differs from the base (%d other files)" % (sha[:12], len(changed))]
            (OUT / "report.txt").write_text("\n".join(report) + "\n")
            print("\n".join(report))
            return 0
        extra = (string_langs(sha, i18n) | named_langs(sha, i18n)) & set(langs)
        probe = [p for p in PROBE if p[0] in themes and p[1] in langs]
        probe += [("light" if "light" in themes else themes[0], l) for l in sorted(extra)]
        probe = list(dict.fromkeys(probe)) or pairs[:1]
        note.append("targeted: first look in %s" % ", ".join("%s/%s" % p for p in probe))

    hostkey = hashlib.sha256(host().encode()).hexdigest()
    shotkey = hashlib.sha256(repr(sorted(mod.shot_list().items())).encode()).hexdigest()
    keytext = "base %s\ndevice %s\nhost %s\nshots %s\n%s\n" % (sha, args.device, host(), shotkey, inputs())
    key = hashlib.sha256(keytext.encode()).hexdigest()
    cache = cache_root()
    base_dir = cache / ("base-%s-%s" % (sha[:12], key[:16]))
    base_top = base_dir / "frames"
    in_use(base_dir)
    (base_dir / "key.txt").write_text(keytext)
    base_bin = None

    def base_frames(want, top=base_top):
        nonlocal base_bin
        with Lock(base_dir / "lock", "shooting this base"):
            # What a run killed mid-shot left
            for stale in base_dir.glob("shoot-*"):
                shutil.rmtree(stale, ignore_errors=True)
            if args.fresh:
                for old in [base_top, *base_dir.glob("again-*")]:
                    shutil.rmtree(old, ignore_errors=True)
                for old in cache.glob("bin-%s-%s/zyperos" % (sha[:12], hostkey[:10])):
                    old.unlink()
                args.fresh = False
            if set(want) - set(frames(top)):
                cards(mod)
                if base_bin is None:
                    base_bin = base_binary(cache, sha, hostkey, args.jobs)
                print("pixel gate: %s (%d to shoot)" % (
                    "the base's shots" if top == base_top else "  of which not kept yet",
                    len(set(want) - set(frames(top)))), flush=True)
                fill(base_bin, top, want, all_names, args.jobs, args.device)
            return frames(top)

    try:
        if args.warm:
            got = base_frames(wanted(pairs, names))
            print("pixel gate: %d base frames kept in %s" % (len(got), base_dir))
            return 0
        print("pixel gate: building this tree", flush=True)
        binary = build(ROOT, args.jobs)
        cards(mod)
        first = wanted(probe if args.targeted else pairs, names)
        base = base_frames(first)
        print("pixel gate: this tree's shots (%d)" % len(first), flush=True)
        fill(binary, here_out, first, all_names, args.jobs, args.device)
        here = frames(here_out)
        if args.targeted:
            moved = sorted({shot_of(k) for k in first
                            if k in base and k in here and not same(base[k], here[k])})
            moved += sorted({shot_of(k) for k in first if (k in base) != (k in here)} - set(moved))
            note.append("%d of %d screens changed in it, shot in every language and theme asked" % (
                len(moved), len(names)))
            if moved:
                rest = wanted(pairs, moved)
                base = base_frames(rest | first)
                print("pixel gate: this tree's changed screens (%d)" % len(rest - first), flush=True)
                fill(binary, here_out, rest, all_names, args.jobs, args.device)
                here = frames(here_out)
            asked = first | wanted(pairs, moved)
            base = {k: v for k, v in base.items() if k in asked}
            here = {k: v for k, v in here.items() if k in asked}
        else:
            asked = first
            base = {k: v for k, v in base.items() if k in asked}
            here = {k: v for k, v in here.items() if k in asked}

        both = sorted(set(base) & set(here))
        changed = [n for n in both if not same(base[n], here[n])]
        # A second look: what changed, shot again on each side, twice. A frame
        # that differs from its own side's first shot shows something that
        # moves by itself - the time, a clock-driven Zyper, the host's free
        # disk space - and is unsteady, not changed by this tree. The base's
        # looks are kept with its frames.
        unsteady = []
        if changed:
            for look in (1, 2):
                print("pixel gate: the base's changed shots again (%d)" % look, flush=True)
                again = base_frames(set(changed), base_dir / ("again-%d" % look))
                unsteady += [n for n in changed if n not in unsteady and n in again
                             and not same(base[n], again[n])]
            for look in (1, 2):
                print("pixel gate: this tree's changed shots again (%d)" % look, flush=True)
                again_out = OUT / ("again-here-%d" % look)
                fill(binary, again_out, set(changed), all_names, args.jobs, args.device)
                again = frames(again_out)
                unsteady += [n for n in changed if n not in unsteady and n in again
                             and not same(here[n], again[n])]
            changed = [n for n in changed if n not in unsteady]
        new = sorted(set(here) - set(base))
        gone = sorted(set(base) - set(here))
        report = ["pixel gate: %d of %d frames changed (base %s, %s, %s)%s" % (
            len(changed), len(both), sha[:12], " ".join(themes), " ".join(langs),
            "; %d more unsteady: each differs from its own side's shot again" % len(unsteady) if unsteady else "")]
        report += ["pixel gate: " + n for n in note]
        # A changed frame of a screen that is unsteady elsewhere in the run
        # (a clock-driven Zyper whose looks happened to agree on each side)
        # stays changed, for a person to judge on its sheet, but says so
        moving = {}
        for n in unsteady:
            moving[shot_of(n)] = moving.get(shot_of(n), 0) + 1
        report += ["changed: " + n + (" (its screen is unsteady in %d other frames)" % moving[shot_of(n)]
                                      if shot_of(n) in moving else "") for n in changed]
        report += ["unsteady: " + n for n in unsteady]
        report += ["new: " + n for n in new]
        report += ["gone: " + n for n in gone]
        made = sheets(changed, base, here, OUT) if changed else []
        report += ["sheet: " + str(p) for p in made]
        (OUT / "report.txt").write_text("\n".join(report) + "\n")
        print("\n".join(report))
        if changed and not args.allow_changes:
            return 1
        return 0
    except KeyboardInterrupt:
        print("pixel gate: stopped", file=sys.stderr)
        return 130
    finally:
        if not args.keep_frames:
            for old in [here_out, *OUT.glob("again-*")]:
                shutil.rmtree(old, ignore_errors=True)
        for top in CARDS:
            shutil.rmtree(top, ignore_errors=True)
        prune(cache, {base_dir})


if __name__ == "__main__":
    sys.exit(main())
