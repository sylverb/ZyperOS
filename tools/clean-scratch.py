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
its own), anything newer than the age, nor anything a running process holds
open (a file in it, or its current folder there: lsof, asked once a run).
Where lsof cannot say, it deletes nothing. Standard library only.
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


def open_paths():
    """Every folder that holds something a running process has open now
    (an open file, its current folder, its program), as real paths; None
    when lsof cannot say. lsof sees this user's processes, which is every
    session's."""
    try:
        r = subprocess.run(["lsof", "-nP", "-w", "-F", "n"], stdout=subprocess.PIPE,
                           stderr=subprocess.DEVNULL, text=True, errors="replace")
    except OSError:
        return None
    # 1 is lsof's answer when it could not look at some process; the rest
    # is still listed
    if r.returncode not in (0, 1) or not r.stdout:
        return None
    held = set()
    for line in r.stdout.splitlines():
        if not line.startswith("n/"):
            continue
        p = line[1:].split(" (", 1)[0]
        while p not in held and p != "/":
            held.add(p)
            p = os.path.dirname(p)
    return held


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
    now = time.time()
    cutoff = now - args.age * 3600
    total = count = busy = 0
    trees = sorted(worktrees(), key=lambda t: -len(str(t)))
    held = open_paths()
    if held is None and not args.dry_run:
        print("clean-scratch: lsof cannot say what is open, so nothing is deleted (-n lists it)")
        args.dry_run = True
    for path in candidates():
        last = newest(path)
        if last >= cutoff:
            continue
        if held is not None and os.path.realpath(path) in held:
            busy += 1
            print("in use, kept %5.0f h  %s" % ((now - last) / 3600, path))
            continue
        n = size(path)
        total += n
        count += 1
        # Whose: the worktree it is in (its folder's name, as sessions name
        # theirs), or the temporary folder's
        owner = next((t.name for t in trees if str(path).startswith(str(t) + os.sep)), "temporary folder")
        print("%s %8.1f MB  %5.0f h  %-28s %s" % ("would delete" if args.dry_run else "deleted",
                                                 n / 1e6, (now - last) / 3600, owner, path))
        if not args.dry_run:
            remove(path)
    if not args.dry_run:
        subprocess.run(["git", "worktree", "prune"], cwd=ROOT)
    print("clean-scratch: %d folders, %.1f GB%s%s" % (count, total / 1e9,
                                                      " to delete (-n)" if args.dry_run else " deleted",
                                                      ", %d in use kept" % busy if busy else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
