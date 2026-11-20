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
each one's picture.

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
SCRIPT = ",".join([
    # Run A: 16 arrivals, about 10 with the page open, some behind it, none
    # in the game, the rest after it opens again
    "wait=300", "fake=16", "wait=900", "list=before",
    "B", "away=450", "game=800", "open", "list=back",
    "wait=1500", "list=later",
    "run", "wait=500", "list=done",
    "B", "B", "away=1", "open", "list=hub", "B",
    # Run B: 20 arrivals, ended behind the page, the renderer gone meanwhile
    "open", "fake=20", "wait=300", "B", "away=4000", "video-gone",
    "open", "list=wall", "wait=1500", "list=wall-later", "B",
])

CAME = re.compile(r"downloads-test: came (\S+) view=(\d+) n=(\d+) arrived=(\d+)")
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
                                 "items": [], "run": len(runs) - 1}
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

    if not need(r.returncode == 0, "the launcher ended with %d" % r.returncode):
        pass
    if not need(len(runs) == 2 and all(name in lists for name in
                ("before", "back", "later", "done", "wall", "wall-later")),
                "the walk did not get through (runs %d, lists %s)" % (len(runs), sorted(lists))):
        for f in fails:
            print("FAIL " + f)
        print("(log: %s)" % log)
        return 1

    def ids(l):
        return [it["id"] for it in l["items"]]

    before, back, later, done = lists["before"], lists["back"], lists["later"], lists["done"]
    # The page open: what it shows is the run so far (the walk itself), each
    # with its picture but the newest, whose picture may be on its way
    need(ids(before) == expect(runs, before) and all(it["tex"] for it in before["items"][1:]),
         "before leaving, the page shows the run's %d arrivals with their pictures (it shows %d)"
         % (before["arrived"], before["n"]))
    away = behind.get("away=450", [{}])[0]
    game = behind.get("game=800", [{}])[0]
    need(away.get("arrived", 0) > before["arrived"] and away.get("live"),
         "arrivals come behind the page (%d before, %s after)" % (before["arrived"], away.get("arrived")))
    need(game.get("arrived") == away.get("arrived"), "nothing comes while a game runs (%s then %s)"
         % (away.get("arrived"), game.get("arrived")))
    # Opened again: every arrival so far, those it showed with their pictures at once
    need(back["view"] == 4, "opened again during the run, the page is on the run (view %d)" % back["view"])
    need(ids(back) == expect(runs, back),
         "opened again, the page lists the run's %d arrivals so far, newest first (it lists %d)"
         % (min(back["arrived"], KEEP), back["n"]))
    was = {it["id"]: it for it in before["items"]}
    for it in back["items"]:
        if it["id"] in was:
            need(it["tex"] and it["size"] == was[it["id"]]["size"],
                 "%s (%s) has its picture back at once, as it was" % it["id"])
    need(ids(later) == expect(runs, later) and all(it["tex"] for it in later["items"][1 if later["view"] == 4 else 0:]),
         "a moment later every one of the %d has its picture (%d of %d do)"
         % (later["n"], sum(it["tex"] for it in later["items"]), later["n"]))
    # Done: the run's newest 14 with their pictures
    need(done["view"] == 5 and done["arrived"] == 16 and ids(done) == expect(runs, done)
         and all(it["tex"] for it in done["items"]),
         "done's wall is the run's newest %d of 16, with their pictures (it has %d)" % (KEEP, done["n"]))
    hub = lists.get("hub")
    need(hub is not None and hub["view"] == 0 and hub["n"] == 0,
         "after done was seen, the page opens on the hub, nothing kept (%s)" % (hub and hub["n"]))
    # Run B: ended behind the page, the renderer gone meanwhile
    wall, wall_later = lists["wall"], lists["wall-later"]
    need(wall["view"] == 5 and wall["arrived"] == 20 and ids(wall) == expect(runs, wall),
         "a run that ended behind the page opens on done with its newest %d of 20, its own (it has %d)"
         % (KEEP, wall["n"]))
    need(ids(wall_later) == expect(runs, wall_later) and all(it["tex"] for it in wall_later["items"]),
         "after the renderer went, the pictures come back (%d of %d)"
         % (sum(it["tex"] for it in wall_later["items"]), wall_later["n"]))
    let_go = behind.get("away=1", [{}])[0].get("kept")
    need(let_go == 0, "the list is let go once its run's end was seen (%s kept)" % let_go)
    for f in fails:
        print("FAIL " + f)
    if fails:
        print("(log: %s)" % log)
        return 1
    print("ok: left with B and opened again, the page lists the run's %d arrivals so far, the %d it showed with "
          "their pictures at once and the rest a moment later; none came in the game; done's wall is the newest "
          "%d of 16; a run ended behind the page shows its own, its pictures back after the renderer went"
          % (min(back["arrived"], KEEP), len([i for i in back["items"] if i["id"] in was]), KEEP))
    return 0


if __name__ == "__main__":
    sys.exit(main())
