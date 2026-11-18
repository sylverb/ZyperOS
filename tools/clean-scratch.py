#!/usr/bin/env python3
"""Deletes what ZyperOS's tools left behind and nothing has touched for a day.

    python3 tools/clean-scratch.py          deletes it (make clean-scratch)
    python3 tools/clean-scratch.py -n       lists it, with sizes, and deletes nothing
    python3 tools/clean-scratch.py --age 6  what nothing touched for 6 hours

What it takes, each only when nothing in its first levels changed for the age:

  - the scratch folders the tools make in the system's temporary folder,
    $TMPDIR/zyperos-*: tools/i18n-shots.py's scratch cards (about 250 MB a
    language and handheld, so 4 GB a check-i18n-shots), the pixel gate's
    base worktree, the README shots' cards, the checks' own. A run that is
    stopped hard (a SIGKILL, a closed laptop's session) leaves them.
  - in every worktree of this repository, the pixel gate's frames
    (build-native/pixel-gate/: the bases each worktree kept before they were
    shared, about 900 MB each, this tree's frames and the second looks); the
    report and the sheets stay.
  - git's records of worktrees whose folders are gone (git worktree prune).

Never a worktree, a branch, the shared gate cache (tools/pixel-gate.py prunes
its own), nor anything newer than the age. Standard library only.
"""
import argparse
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def newest(path, depth=3):
    """The latest change in path and its first levels: a run that is still
    writing (a shot's .last, a frame) is recent somewhere in them"""
    try:
        latest = path.lstat().st_mtime
    except OSError:
        return 0
    if depth == 0 or not path.is_dir() or path.is_symlink():
        return latest
    try:
        with os.scandir(path) as it:
            for entry in it:
                latest = max(latest, newest(Path(entry.path), depth - 1))
    except OSError:
        pass
    return latest


def size(path):
    r = subprocess.run(["du", "-sk", str(path)], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    try:
        return int(r.stdout.split()[0]) * 1024
    except (IndexError, ValueError):
        return 0


def remove(path):
    def writable(func, p, exc):
        # A card's read-only file (a fixture copied with its mode)
        try:
            os.chmod(os.path.dirname(p), stat.S_IRWXU)
            os.chmod(p, stat.S_IRWXU)
            func(p)
        except OSError:
            pass
    if path.is_dir() and not path.is_symlink():
        shutil.rmtree(path, onerror=writable)
    else:
        path.unlink(missing_ok=True)


def worktrees():
    r = subprocess.run(["git", "worktree", "list", "--porcelain"], cwd=ROOT, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return [Path(l[9:]) for l in r.stdout.splitlines() if l.startswith("worktree ")]


def candidates():
    tmp = Path(os.environ.get("TMPDIR") or tempfile.gettempdir())
    for p in sorted(tmp.glob("zyperos-*")):
        yield p
    for tree in worktrees():
        gate = tree / "build-native" / "pixel-gate"
        for pattern in ("base-*", "here", "again-*"):
            for p in sorted(gate.glob(pattern)):
                yield p


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("-n", "--dry-run", action="store_true", help="list, delete nothing")
    ap.add_argument("--age", type=float, default=24, help="hours untouched (24)")
    args = ap.parse_args()
    cutoff = time.time() - args.age * 3600
    total = count = 0
    for path in candidates():
        if newest(path) >= cutoff:
            continue
        n = size(path)
        total += n
        count += 1
        print("%s %8.1f MB  %s" % ("would delete" if args.dry_run else "deleted", n / 1e6, path))
        if not args.dry_run:
            remove(path)
    if not args.dry_run:
        subprocess.run(["git", "worktree", "prune"], cwd=ROOT)
    print("clean-scratch: %d folders, %.1f GB%s" % (count, total / 1e9,
                                                    " to delete (-n)" if args.dry_run else " deleted"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
