"""Zipped games by every method a zip tool offers, read by src/zip.c: stored,
Deflate and LZMA (7-Zip's method 14) give the ROM whole; bzip2 (12), which
the system has no library for, is refused - never the readme beside it, which
is the largest file this can read. Each zip as a No-Intro set comes: the ROM
and a small readme. Python's zipfile writes them (its LZMA is the APPNOTE's,
with the end marker), so nothing here is a file someone else made.

    python3 test/zipmethods.py BUILD/zipload
"""
import os, random, subprocess, sys, tempfile, zipfile

tool = sys.argv[1]
rom = bytes(random.Random(7).randrange(256) for _ in range(40000)) + b"\0" * 25536
readme = b"A game, one ROM per archive.\n" * 4


def fnv(data):
    h = 2166136261
    for b in data:
        h = ((h ^ b) * 16777619) & 0xffffffff
    return "%08x" % h


fails = []
with tempfile.TemporaryDirectory(prefix="diatom-zip-") as tmp:
    zips = []
    for name, method in (("stored", zipfile.ZIP_STORED), ("deflate", zipfile.ZIP_DEFLATED),
                         ("lzma", zipfile.ZIP_LZMA), ("bzip2", zipfile.ZIP_BZIP2)):
        path = os.path.join(tmp, name + ".zip")
        with zipfile.ZipFile(path, "w") as z:
            z.writestr(zipfile.ZipInfo("readme.txt"), readme, compress_type=zipfile.ZIP_DEFLATED)
            z.writestr(zipfile.ZipInfo("Pocket Quest (World).gb"), rom, compress_type=method)
        zips.append(path)
    out = subprocess.run([tool] + zips, capture_output=True, text=True).stdout.splitlines()
    want = "Pocket Quest (World).gb %d %s" % (len(rom), fnv(rom))
    for got, (label, expect) in zip(out, (("stored", want), ("Deflate", want), ("LZMA", want),
                                          ("bzip2, refused, not its readme", "refused"))):
        ok = got == expect
        print(("  ok   " if ok else "  FAIL ") + label + ("" if ok else "\n         got  " + got))
        if not ok:
            fails.append(label)
    if len(out) != 4:
        fails.append("one line a zip")
        print("  FAIL one line a zip, got %d" % len(out))

if fails:
    print("\n%d zip check(s) failed" % len(fails))
    sys.exit(1)
print("\nok: stored, Deflate and LZMA read; bzip2 refused, not its readme")
