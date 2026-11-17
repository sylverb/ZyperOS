#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Every option row keeps one rule (DESIGN.md 9, src/menu.h's menu_choice).

    make check-menu-rows

A row with more than two choices opens its list on A; one with two turns
over; Left and Right step every row. The menu runner does the A part for any
row whose build gives it a table of choices (menu_choose), so the rule holds
when every option row has one, and every table says what its row says.

The real launcher, on the demonstration card's shelves (a console's and MP4's)
in English, prints each option page's rows (--rows-report): MENU, the shelf's
menu, Theme, Night Hours, the in-game menu's Game and Display tabs, Video Snaps,
Menu Music and On-Screen Stats. This fails on:

  - a row that steps (its sound class rings or says A does nothing) with no
    table, unless it is one of two choices (On and Off, or a pair named below)
  - a table whose choice that is on is not what its row says, word for word
    (its list shows the row's words as they are, src/menu_pick.c)
  - a table of one choice, or a list of its own on a row not named below

so a new option row of more than two choices cannot ship without its list.

And in every language, a row's value or a choice of its table that is one of
res/strings' and begins with a small letter: every value beside a row's name
begins with a capital (GLOSSARY.md), "Never" and "Not connected" as "Off"
does. A name keeps its own case (a core's "pcsx_rearmed", a song's), and a
number or a unit begins as it begins. tools/i18n-shots.py holds the same of
every value it draws, on every screen."""
import importlib.util
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("readme_shots", ROOT / "tools" / "readme-shots.py")
R = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(R)
# Its strings by language: which words a value is the table's (small_first)
_spec = importlib.util.spec_from_file_location("i18n_shots", ROOT / "tools" / "i18n-shots.py")
S = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(S)

SND_RING, SND_NO_A = 1, 2
# Steppers of two choices that are not On and Off: they turn over on A as
# their screens have them, and need no list
PAIRS = {"Home Screen", "System Order", "Clock", "Audio Output", "USB Accessories", "View",
         "Corners", "Swap Screens", "Stylus", "Pak", "Power Button"}
# Rows that open a list of their own on A
OWN = {"Language"}
# The languages besides English whose values are held to a capital
LANGS = [code for code in S.i18n.CODES if code != "en"]


def report(binary, card, tag, focus, tmp, lang="en"):
    (card / "ZyperOS/.last").write_text(tag + "\n" + focus + "\n")
    shot = Path(tmp) / ("rows-%s-%s.png" % (tag, lang))
    cmd = [str(binary), "--shot", str(shot), "--lang", lang, "--screen", "systems",
           "--status", "10:08 24h wifi=off batt=82", "--fn-volume", "--rows-report"]
    r = subprocess.run(cmd, cwd=ROOT, env=R.env_for(card), text=True, errors="replace",
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    rows = [l[6:].split("\t") for l in r.stdout.splitlines() if l.startswith("rows: ")]
    assert rows, "%s: no rows reported (exit %d): %s" % (tag, r.returncode, r.stdout[-600:])
    return rows


def capitals(lang, reports, fails):
    """A value or a choice of the table's own words that begins with a small
    letter, once each"""
    said = set()
    for page, label, value, *_, choices in reports:
        for text in [value] + (choices.split("\x1f") if choices else []):
            small = S.strings_of(lang).small_first(text)
            if small and (lang, text) not in said:
                said.add((lang, text))
                fails.append("%s: %s: %s: %r begins with a small letter (%s)"
                             % (lang, page, label, text, "/".join(list(dict.fromkeys(small))[:3])))


def main():
    binary = ROOT / "build-native/zyperos"
    fails, seen = [], 0
    with tempfile.TemporaryDirectory(prefix="zyperos-rows-") as tmp:
        demo, media = Path(tmp) / "demo", Path(tmp) / "media"
        R.demo_card(demo, None, {"artwork": {}})
        R.media_card(media)
        reports = report(binary, demo, "PS1", R.FOCUS + ".chd", tmp) + report(binary, media, "MUSE", "", tmp)
        capitals("en", reports, fails)
        for lang in LANGS:
            capitals(lang, report(binary, demo, "PS1", R.FOCUS + ".chd", tmp, lang)
                     + report(binary, media, "MUSE", "", tmp, lang), fails)
    for page, label, value, live, snd, n, cur, none, own, word, _choices in reports:
        snd, n, cur, none, own = int(snd), int(n), int(cur), int(none), int(own)
        where = "%s: %s" % (page, label)
        if n:
            seen += 1
            if own:
                if label not in OWN:
                    fails.append("%s opens a list of its own; only %s may" % (where, ", ".join(sorted(OWN))))
                continue
            if n < 2:
                fails.append("%s: a table of %d choice" % (where, n))
            # The row says the choice that is on, as the list shows it; with
            # none on as a whole it says something else
            if not none and value and word != value:
                fails.append("%s says %r and its table has %r on" % (where, value, word))
            continue
        if live != "1" or not value or value == "(tick)":
            continue
        # On or Off, alone or before a size ("On · 1.2 GB", a system's Video
        # Snaps): a switch, two choices
        switch = value.split(" · ")[0] in ("On", "Off")
        if snd & (SND_RING | SND_NO_A) and not switch and label not in PAIRS:
            fails.append("%s steps (%r) with no table of choices: give it one (menu.h's menu_choice)"
                         % (where, value))
    for f in fails:
        print("  FAIL " + f)
    if fails:
        sys.exit(1)
    print("ok: %d option rows open their list on A or turn over; every table says what its row says; "
          "every value and choice in %d languages begins with a capital" % (seen, len(LANGS) + 1))


main()
