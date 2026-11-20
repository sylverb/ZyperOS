#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""What came, kept: MENU > Downloads left and opened again (src/dl_screen.c).

    python3 tools/dl-recent-check.py        (make check-dl-recent, after make native)
    python3 tools/dl-recent-check.py --binary PATH    another launcher's walk

The real launcher, headless, on readme-shots' demo card with a cover for every
other game, plays ZYPEROS_DOWNLOADS_TEST's script (dl_test): a stand-in run on
the art lane (fake=N: one of the card's games every 150 ms, its cover where it
has one, else its card), the page left with B (Continue in background), the
lanes stepped behind it as the shelf steps them, a game (they wait), the page
opened again. `list=NAME` says what the page shows came, newest first, with
each one's picture and the page's pass since it opened.

Every wait is for an event, never for a time: the stand-in holds after its
Kth arrival (hold=K), the page waits until every arrival has its picture
(pics), so what it had when B left it is known, not raced; behind the page
it goes on until K more have come (away=K); opened again, the first list is
on the page's first pass (pass=0), before it has read any picture. A loaded
machine only makes it slower (on GitHub's runner, 3 checks at a time, the
first version raced a picture landing between its list and B).

Tarik on 1.1.0-rc.3: left in the background and opened again, the page had
lost the pictures that had come and showed only what came after. This fails
unless the page opened again lists every arrival of the run so far, newest
first and at most 14 (the stream's 7 and done's wall of 14), those it showed
before with the very pictures they had, at once, and the ones that came
behind it with theirs a moment later; nothing comes during the game; done's
wall is the run's newest 14 with their pictures; a run that ends behind the
page shows its wall when the page opens, a new run starting the list again;
and after the renderer went (a game in the fallback, launch_screen.c) the
pictures come back from their files. The list is let go once its run's end
has been seen.
"""
import importlib.util
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("rs", ROOT / "tools" / "readme-shots.py")
R = importlib.util.module_from_spec(spec)
spec.loader.exec_module(R)

KEEP = 14   # DL_RING
BEFORE, AWAY = 5, 3
SCRIPT = ",".join([
    # Run A: 16 arrivals, 5 with the page open and their pictures in, 3
    # behind it, none in the game, the rest after it opens again
    "fake=16", "hold=%d" % BEFORE, "pics", "list=before",
    "B", "away=%d" % AWAY, "game=800", "open", "list=back",
    "run", "pics", "list=done",
    "B", "B", "kept", "open", "list=hub", "B",
    # Run B: 20 arrivals, 3 with the page open, the rest and the end behind
    # it, the renderer gone meanwhile
    "open", "fake=20", "hold=3", "B", "away=20", "video-gone",
    "open", "list=wall", "pics", "list=wall-later", "B",
])

CAME = re.compile(r"downloads-test: came (\S+) view=(\d+) n=(\d+) arrived=(\d+) pass=(\d+)")
ITEM = re.compile(r"downloads-test: came (\S+) (\d+) key=(\d+) kind=(\d+) tex=(yes|no) (\d+)x(\d+)(?: card=\d)? game=(.*)\|folder=(.*)$")
STAND = re.compile(r"downloads-test: stand-in (\d+) game=(.*)\|folder=(.*)$")
BEHIND = re.compile(r"downloads-test: (\S+) \(behind the page\) live=(\d) arrived=(\d+)(?: kept=(\d+))?")


def card_with_covers(card):
    R.demo_card(card, None, {"artwork": {}})
    R.set_settings(card, {"tour.done": "1"}, True)
    n = 0
    for folder, games in R.LIBRARY.items():
        for i, (file, _) in enumerate(games):
            if i % 2:
                continue
            stem = Path(file).stem
            color = ((37 * n) % 200 + 40, (91 * n) % 200 + 40, (53 * n) % 200 + 40)
            R.png_rgb(card / "Roms" / folder / ".media" / (stem + ".png"), 96, 128, [[color] * 96] * 128)
            n += 1


def parse(out):
    runs, lists, behind = [], {}, {}
    for line in out.splitlines():
        m = STAND.match(line)
        if m:
            if int(m.group(1)) == 0:
                runs.append([])
            runs[-1].append((m.group(2), m.group(3)))
            continue
        m = CAME.match(line)
        if m:
            lists[m.group(1)] = {"view": int(m.group(2)), "n": int(m.group(3)), "arrived": int(m.group(4)),
                                 "pass": int(m.group(5)), "items": [], "run": len(runs) - 1}
            continue
        m = ITEM.match(line)
        if m and m.group(1) in lists:
            lists[m.group(1)]["items"].append({"key": int(m.group(3)), "tex": m.group(5) == "yes",
                                               "size": (int(m.group(6)), int(m.group(7))),
                                               "id": (m.group(8), m.group(9))})
            continue
        m = BEHIND.match(line)
        if m:
            behind.setdefault(m.group(1), []).append({"live": m.group(2) == "1", "arrived": int(m.group(3)),
                                                      "kept": None if m.group(4) is None else int(m.group(4))})
    return runs, lists, behind


def expect(runs, l, arrived=None):
    """The run's arrivals the list should hold: the newest KEEP, newest first"""
    seq = runs[l["run"]]
    got = l["arrived"] if arrived is None else arrived
    return [seq[k] for k in range(got - 1, max(got - KEEP, 0) - 1, -1)]


def main():
    binary = ROOT / "build-native" / "zyperos"
    if len(sys.argv) == 3 and sys.argv[1] == "--binary":
        binary = Path(sys.argv[2]).resolve()
    if subprocess.call(["pkg-config", "--exists", "sdl2", "SDL2_image", "SDL2_ttf"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
        print("skipped: no SDL2 here to build the launcher with")
        return 0
    if not binary.is_file():
        print("FAIL no build-native/zyperos: make native first")
        return 1
    with tempfile.TemporaryDirectory(prefix="zyperos-dl-recent-") as tmp:
        card = Path(tmp) / "card"
        card_with_covers(card)
        env = dict(R.env_for(card), ZYPEROS_DOWNLOADS_TEST=SCRIPT)
        r = subprocess.run([str(binary), "--lang", "en"], cwd=ROOT, env=env, text=True, errors="replace",
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=180)
    log = ROOT / "build-native" / "dl-recent-check.log"
    log.write_text(r.stdout)
    runs, lists, behind = parse(r.stdout)
    fails = []

    def need(cond, what):
        if not cond:
            fails.append(what)
        return cond

    need(r.returncode == 0, "the launcher ended with %d" % r.returncode)
    if not need(len(runs) == 2 and all(name in lists for name in
                ("before", "back", "done", "hub", "wall", "wall-later")),
                "the walk did not get through (runs %d, lists %s)" % (len(runs), sorted(lists))):
        return report(fails, log, r.stdout)

    def ids(l):
        return [it["id"] for it in l["items"]]

    before, back, done = lists["before"], lists["back"], lists["done"]
    # The page open, the stand-in holding after its 5th, every picture in:
    # what the page has as B leaves it
    need(before["arrived"] == BEFORE and ids(before) == expect(runs, before)
         and all(it["tex"] for it in before["items"]),
         "before leaving, the page shows the run's %d arrivals, each with its picture (it shows %d, %d with one)"
         % (BEFORE, before["n"], sum(it["tex"] for it in before["items"])))
    away = behind.get("away=%d" % AWAY, [{}])[0]
    game = behind.get("game=800", [{}])[0]
    need(away.get("arrived") == BEFORE + AWAY and away.get("live"),
         "%d arrivals come behind the page (%d before, %s after)" % (AWAY, BEFORE, away.get("arrived")))
    need(game.get("arrived") == away.get("arrived"), "nothing comes while a game runs (%s then %s)"
         % (away.get("arrived"), game.get("arrived")))
    # Opened again, on its first pass: every arrival so far, the ones it had
    # with their very pictures, before it has read one
    need(back["view"] == 4 and back["pass"] == 0,
         "opened again during the run, the page is on the run, listed on its first pass (view %d, pass %d)"
         % (back["view"], back["pass"]))
    need(ids(back) == expect(runs, back),
         "opened again, the page lists the run's %d arrivals so far, newest first (it lists %d)"
         % (min(back["arrived"], KEEP), back["n"]))
    was = {it["id"]: it for it in before["items"]}
    for it in back["items"]:
        if it["id"] in was:
            need(it["tex"] and it["size"] == was[it["id"]]["size"],
                 "%s (%s) has its picture back at once, as it was" % it["id"])
    # Done: the run's newest 14, the ones that came behind the page among
    # them, every one with its picture
    need(done["view"] == 5 and done["arrived"] == 16 and ids(done) == expect(runs, done)
         and all(it["tex"] for it in done["items"]),
         "done's wall is the run's newest %d of 16, with their pictures (it has %d, %d with one)"
         % (KEEP, done["n"], sum(it["tex"] for it in done["items"])))
    hub = lists["hub"]
    need(hub["view"] == 0 and hub["n"] == 0,
         "after done was seen, the page opens on the hub, nothing kept (%d)" % hub["n"])
    let_go = behind.get("kept", [{}])[0].get("kept")
    need(let_go == 0, "the list is let go once its run's end was seen (%s kept)" % let_go)
    # Run B: ended behind the page, the renderer gone meanwhile
    wall, wall_later = lists["wall"], lists["wall-later"]
    need(wall["view"] == 5 and wall["arrived"] == 20 and ids(wall) == expect(runs, wall),
         "a run that ended behind the page opens on done with its newest %d of 20, its own (it has %d)"
         % (KEEP, wall["n"]))
    need(ids(wall_later) == expect(runs, wall_later) and all(it["tex"] for it in wall_later["items"]),
         "after the renderer went, the pictures come back (%d of %d)"
         % (sum(it["tex"] for it in wall_later["items"]), wall_later["n"]))
    if fails:
        return report(fails, log, r.stdout)
    print("ok: left with B and opened again, the page lists the run's %d arrivals so far on its first pass, the %d "
          "it had with their very pictures, and the %d that came behind it get theirs; none came in the game; "
          "done's wall is the newest %d of 16; a run ended behind the page shows its own, its pictures back after "
          "the renderer went" % (back["arrived"], BEFORE, AWAY, KEEP))
    return 0


def report(fails, log, out):
    """The failures, and the walk's own lines with them: on a CI runner the
    log file is gone with the runner"""
    for f in fails:
        print("FAIL " + f)
    print("the walk said:")
    for line in out.splitlines():
        if line.startswith("downloads-test:") and " stand-in " not in line:
            print("  " + line[len("downloads-test: "):])
    print("(log: %s)" % log)
    return 1


if __name__ == "__main__":
    sys.exit(main())
