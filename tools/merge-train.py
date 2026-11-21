#!/usr/bin/env python3
"""A merge train: several pull requests merged onto main at once, checked once.

    python3 tools/merge-train.py 281 283 284
                    merges each PR's head onto origin/main, in that order, in
                    a scratch worktree of its own; joins the check lists they
                    all add to (the Makefile's CHECKS and .PHONY, the table in
                    docs/building.md) and writes again what is generated
                    (tools/i18n.py, tools/designdef.py); runs the gate: one
                    make check, then CI's Linux job and its device compile
                    in Ubuntu 24.04 in Docker, under a check slot; keeps the
                    branch train-281-283-284 here
    python3 tools/merge-train.py --integrate-only 281 283 284
                    the merges alone, the worktree kept and its path printed:
                    what a test card is built from, before anything is checked
    python3 tools/merge-train.py --merge 281 283 284
                    the gate, then each PR merged on GitHub in that order
                    (gh pr merge N --merge), each still at the head the gate
                    took; a PR that needed its lists joined gets main merged
                    into its branch first, as the train joined them, so main
                    ends at the tree the gate checked
    python3 tools/merge-train.py --push 281 283 284
                    the gate, then the branch pushed and its own pull request
                    opened, which merged with a merge commit lands the PRs
                    together
    --no-check      the merges and the generated files, no gate
    --no-docker     the gate without its Linux half (make check alone)
    --recheck       run the gate even when these heads on this main passed it
    --keep          the worktree stays (its path is printed)
    --name NAME     the train's branch (train-<numbers>, -2 and on if taken)

Merged one at a time, every merge to main sent each other open pull request
back to merge main, write the strings again and run the whole make check
(twenty minutes and more on a loaded Mac, 35 on CI), and the files they all
add a line to conflicted after each merge: on 2026-11-20 #421 twice, #422 and
#428 once, each a new CI run. The train merges them in one go. A conflict in
a file a generator writes whole (src/i18n_ids.h, res/web/i18n/,
src/design.h ...) is settled by writing the file again from the merged
sources; one in a check list (the Makefile's CHECKS and .PHONY, made only of
names, or docs/building.md's rows of checks) by keeping both sides' names and
rows; a conflict anywhere else, or in a list's prose or rules, stops the
train at that PR, which then needs main merged in by hand.

The gate is the CI the PRs would have had, here and once for all: a fresh
worktree, so a fresh build-native/ (no object of another tree's), make check
on this Mac, and the Linux job's own commands (read from
.github/workflows/check.yml: check-linux's make check with its CHECKS_SKIP,
and the devices job's make check-device-compile) in ubuntu:24.04 set up by
tools/ubuntu-setup.sh, the image built once and kept (zyperos-ubuntu-ci,
tagged by that script's hash), on a copy of the tree, as a user that is not
root as on CI's runner. arm64 on an Apple Mac, where CI is x86_64. It takes
one of the check slots first (/tmp/zyperos-check-1 or -2, an owner file,
looking every two minutes while both are taken), and lets it go when it
ends. CHECK_JOBS goes to both as it is (Docker: 4 unless it is set).

What it found is kept (~/Library/Caches/ZyperOS/merge-train, a report and
its record): the base, each PR's head, the train's tree, each part's verdict
and time. --merge with the same main and heads as a gate that passed merges
without running it again, so the gate can run before a test card is built
and the merge after the card is tested. --merge refuses if main moved
since, or a PR's head did, and stops at the first PR whose merge does not
land the tree the gate checked.

Nothing is pushed without --push or --merge, and main is never pushed to:
--merge merges each PR on GitHub, and pushes only a merge of main into a
PR's own branch. Merged with a merge commit (gh pr merge N --merge; not a
squash or a rebase), each PR's commits are in main and GitHub marks each PR
merged. Needs git 2.38 or later and gh, and Docker for the Linux half.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]

# The files a generator writes whole, by the generator: a conflict in one of
# these is settled by running it over the merged sources. Not the files it
# writes a part of (DESIGN.md's tables, the pages' CSS blocks): a conflict
# there may be in the prose around, and is a hand's.
GENERATED = {
    ("python3", "tools/i18n.py"): ("src/i18n_ids.h", "src/i18n_tables.c", "res/web/i18n/"),
    ("python3", "tools/designdef.py"): ("src/design.h", "src/ds/layout.h", "docs/design/mock/tokens.js"),
}

# The lists every PR that adds a check adds a line to, side by side: joined
# (join_makefile, join_rows), never chosen between
LISTS = ("Makefile", "docs/building.md")

SLOTS = [Path("/tmp/zyperos-check-1"), Path("/tmp/zyperos-check-2")]
IMAGE = "zyperos-ubuntu-ci"
if sys.platform == "darwin":
    KEPT = Path.home() / "Library" / "Caches" / "ZyperOS" / "merge-train"
else:
    KEPT = Path(os.environ.get("XDG_CACHE_HOME") or Path.home() / ".cache") / "zyperos" / "merge-train"


class Stop(Exception):
    """The train stops here, said why"""


def git(*args, cwd=ROOT, check=True, capture=True, env=None):
    r = subprocess.run(["git", *args], cwd=cwd, text=True, env=env,
                       stdout=subprocess.PIPE if capture else None,
                       stderr=subprocess.PIPE if capture else None)
    if check and r.returncode:
        raise Stop("git %s: %s" % (" ".join(args), (r.stderr or "").strip()))
    return r


def gh_json(*args):
    r = subprocess.run(["gh", *args], cwd=ROOT, text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
    if r.returncode:
        raise Stop("gh %s: %s" % (" ".join(args), r.stderr.strip()))
    return json.loads(r.stdout)


def generated(path):
    """The generator whose output path is, or None"""
    for gen, outs in GENERATED.items():
        if any(path == o or (o.endswith("/") and path.startswith(o)) for o in outs):
            return gen
    return None


def regenerate(tree, gens):
    for gen in gens:
        if (tree / gen[1]).is_file():
            r = subprocess.run(list(gen), cwd=tree, text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
            if r.returncode:
                print(r.stdout[-2000:], file=sys.stderr)
                raise Stop("%s failed" % " ".join(gen))


def ignored(tree, path):
    """Whether the train's .gitignore keeps path out (src/i18n_tables.c,
    since the build writes it)"""
    return git("check-ignore", "-q", "--no-index", "--", path, cwd=tree, check=False).returncode == 0


# ---- the check lists ----------------------------------------------------------

HUNK = re.compile(r"^<<<<<<< [^\n]*\n(.*?)^=======\n(.*?)^>>>>>>> [^\n]*\n", re.S | re.M)
# A line of a make list: names, a continuation, at most the list's own start
LIST_LINE = re.compile(r"^\s*(?:\.PHONY:|CHECKS\s*=)?\s*(?:[\w.$()-]+(?:\s+[\w.$()-]+)*)?\s*\\?\s*$")
LIST_START = re.compile(r"^(?:\.PHONY:|CHECKS\s*=)")
MARK = re.compile(r"^(?:<<<<<<<|=======|>>>>>>>)")
ROW = re.compile(r"^\| `([\w-]+)` \|.*\|\s*$")


def words(text):
    return [w for w in re.findall(r"\S+", re.sub(r"^\s*(?:\.PHONY:|CHECKS\s*=)", "", text, flags=re.M))
            if w != "\\"]


def join_makefile(text):
    """The Makefile with every conflict between two check lists joined: our
    side as it is, each name only theirs has put after the name it follows
    there (first, when it is first). None when a conflict is anything but
    names, so a rule or a variable is never joined."""
    def in_list(m):
        """The hunk starts one of the two lists, or is inside one: the lines
        before it run on (a backslash) back to the list's start"""
        if any(LIST_START.match(side) for side in (m.group(1), m.group(2))):
            return True
        before = m.string[:m.start()].splitlines()
        k = len(before)
        while k > 0 and (before[k - 1].rstrip().endswith("\\") or MARK.match(before[k - 1])):
            k -= 1
        return k < len(before) and bool(LIST_START.match(before[k]))

    def one(m):
        ours, theirs = m.group(1), m.group(2)
        lines = (ours + theirs).splitlines()
        if not lines or not all(LIST_LINE.match(line) for line in lines) or not in_list(m):
            raise Stop("not a list")
        if not ours.strip():
            return theirs
        have, tw = words(ours), words(theirs)
        out = ours
        for i, w in enumerate(tw):
            if w in have:
                continue
            before = next((tw[j] for j in range(i - 1, -1, -1) if tw[j] in have), None)
            if before is None:
                # first of the hunk: before our first name
                at = re.match(r"\s*(?:\.PHONY:\s*|CHECKS\s*=\s*)?", out).end()
                out = out[:at] + w + " " + out[at:]
            else:
                out = re.sub(r"(?<!\S)%s(?!\S)" % re.escape(before), lambda _: before + " " + w, out, count=1)
            have.insert(have.index(before) + 1 if before else 0, w)
        return out
    try:
        return HUNK.sub(one, text)
    except Stop:
        return None


def join_rows(text):
    """docs/building.md with every conflict between rows of the checks' table
    joined: our rows, then each row of a check only theirs has. None when a
    conflict holds prose, or both sides wrote the same check's row
    differently (a real conflict, a hand's)."""
    def one(m):
        ours, theirs = m.group(1).splitlines(True), m.group(2).splitlines(True)
        if not all(ROW.match(line) for line in ours + theirs):
            raise Stop("not rows")
        mine = {ROW.match(line).group(1): line for line in ours}
        out = list(ours)
        for line in theirs:
            name = ROW.match(line).group(1)
            if name not in mine:
                out.append(line)
            elif mine[name] != line:
                raise Stop("the same row twice")
        return "".join(out)
    try:
        return HUNK.sub(one, text)
    except Stop:
        return None


def make_lists(text):
    """The names in the Makefile's .PHONY and CHECKS lists"""
    flat = text.replace("\\\n", " ")
    got = {}
    for key, pat in (("phony", r"^\.PHONY:(.*)$"), ("checks", r"^CHECKS = (.*)$")):
        m = re.search(pat, flat, re.M)
        got[key] = set(m.group(1).split()) if m else set()
    return got


def join_lists(tree, path):
    """Join path's conflicts if it is a check list and each is only names or
    rows; True when it was, the file written and staged. Each side's names
    and rows must all be in what it makes."""
    if path not in LISTS:
        return False
    text = (tree / path).read_text()
    joined = join_makefile(text) if path == "Makefile" else join_rows(text)
    if joined is None or "<<<<<<<" in joined or ">>>>>>>" in joined:
        return False
    sides = [git("show", ":%d:%s" % (stage, path), cwd=tree, check=False).stdout for stage in (2, 3)]
    if path == "Makefile":
        got = make_lists(joined)
        for side in sides:
            for key, names in make_lists(side).items():
                if not names <= got[key]:
                    return False
    else:
        got = set(ROW.match(line).group(1) for line in joined.splitlines() if ROW.match(line))
        for side in sides:
            if not {ROW.match(line).group(1) for line in side.splitlines() if ROW.match(line)} <= got:
                return False
    (tree / path).write_text(joined)
    git("add", "--", path, cwd=tree)
    return True


def merge_one(tree, ref, message):
    """Merge ref into tree's HEAD with a merge commit; conflicts in the check
    lists joined and in generated files written again. What was settled, a
    list of (path, how); Stop naming the files when another conflict is
    left."""
    r = git("-c", "merge.conflictStyle=merge", "merge", "--no-ff", "-m", message, ref, cwd=tree, check=False)
    if r.returncode == 0:
        return []
    conflicted = git("diff", "--name-only", "--diff-filter=U", cwd=tree).stdout.split()
    if not conflicted:
        git("merge", "--abort", cwd=tree, check=False)
        raise Stop("did not merge: %s" % (r.stderr.strip() or r.stdout.strip()))
    settled, gens, hand = [], [], []
    for p in conflicted:
        gen = generated(p)
        if gen:
            if gen not in gens:
                gens.append(gen)
            if ignored(tree, p):
                git("rm", "-q", "--cached", "--ignore-unmatch", "--", p, cwd=tree)
            else:
                git("checkout", "--ours", "--", p, cwd=tree, check=False)
            settled.append((p, "written again"))
        elif join_lists(tree, p):
            settled.append((p, "both sides' checks kept"))
        else:
            hand.append(p)
    if hand:
        git("merge", "--abort", cwd=tree, check=False)
        raise Stop("conflicts in %s: merge main (and the PRs before it here) into it by hand, "
                   "or leave it out" % ", ".join(hand))
    # Only generated files and lists: each generated one written again from
    # the merged sources. The string tables are no longer committed: where
    # the train's main ignores them, a PR from before that carries them
    # loses them here.
    regenerate(tree, gens)
    for p, how in settled:
        if how == "written again" and (tree / p).exists() and not ignored(tree, p):
            git("add", "--", p, cwd=tree)
    git("commit", "-q", "--no-edit", cwd=tree)
    return settled


def integrate(tree, prs, refs):
    """Each PR merged onto tree's HEAD in order, then whatever merged cleanly
    but is stale written again (two PRs that each added a string merge
    without a conflict and leave the IDs a step behind). The train's commit
    after each PR (its tree is what main must hold after that PR merges),
    and what was settled."""
    after, notes = [], []
    for pr, ref in zip(prs, refs):
        n = pr["number"]
        owner = (pr.get("headRepositoryOwner") or {}).get("login", "")
        message = "Merge pull request #%d from %s/%s" % (n, owner, pr["headRefName"])
        try:
            settled = merge_one(tree, ref, message)
        except Stop as e:
            raise Stop("#%d %s" % (n, e))
        said = ("merged" if not settled else "merged, " + "; ".join("%s %s" % (p, how) for p, how in settled))
        print("merge train: #%d %s: %s" % (n, pr["title"], said), flush=True)
        notes.append("#%d %s (head %s): %s" % (n, pr["title"], pr["headRefOid"][:12], said))
        after.append(git("rev-parse", "HEAD", cwd=tree).stdout.strip())
    regenerate(tree, list(GENERATED))
    outs = [o for outs in GENERATED.values() for o in outs
            if (tree / o).exists() and not ignored(tree, o.rstrip("/"))]
    git("add", "-A", "--", *outs, cwd=tree)
    if git("diff", "--cached", "--quiet", cwd=tree, check=False).returncode:
        git("commit", "-q", "-m", "Write the generated files again after the train's merges", cwd=tree)
        print("merge train: the generated files written again (a commit of their own)")
        notes.append("the generated files written again after the merges (a commit of their own)")
        after[-1] = git("rev-parse", "HEAD", cwd=tree).stdout.strip()
    return after, notes


# ---- the gate -----------------------------------------------------------------

def take_slot(name, what):
    """One of the check slots (helpers' rule: mkdir, an owner file, look
    every two minutes while both are taken, never take one over)"""
    said = 0
    while True:
        for slot in SLOTS:
            try:
                slot.mkdir()
            except FileExistsError:
                continue
            (slot / "owner").write_text("merge train %s (pid %d)\n%s\n%s\n" % (
                name, os.getpid(), what, time.strftime("%a %b %d %H:%M:%S %Z %Y")))
            print("merge train: took %s" % slot, flush=True)
            return slot
        if time.time() - said > 600:
            owners = []
            for slot in SLOTS:
                try:
                    owners.append((slot / "owner").read_text().splitlines()[0])
                except (OSError, IndexError):
                    owners.append("?")
            print("merge train: both check slots taken (%s); looking again every two minutes"
                  % "; ".join(owners), flush=True)
            said = time.time()
        time.sleep(120)


def give_slot(slot):
    if slot:
        shutil.rmtree(slot, ignore_errors=True)


def ci_commands(tree):
    """The Linux jobs' make commands, as .github/workflows/check.yml has
    them: check-linux's make check (its CHECKS_SKIP) and the devices job's
    make check-device-compile, each its job's name and the line"""
    got, job, lines = [], None, (tree / ".github/workflows/check.yml").read_text().splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        m = re.match(r"^  ([\w-]+):\s*$", line)
        if m:
            job = m.group(1)
        m = re.match(r"^(\s*(?:- )?)run:\s*(.*)$", line)
        if m and job in ("check-linux", "devices"):
            indent, value = len(m.group(1)), m.group(2).strip()
            if value[:1] in (">", "|"):
                body = []
                while i + 1 < len(lines) and (not lines[i + 1].strip() or
                                              len(lines[i + 1]) - len(lines[i + 1].lstrip()) > indent):
                    i += 1
                    body.append(lines[i].strip())
                value = (" " if value[0] == ">" else "\n").join(b for b in body if b)
            if value.startswith("make "):
                got.append((job, value))
        i += 1
    if not any(n == "check-linux" for n, _ in got) or not any(n == "devices" for n, _ in got):
        raise Stop("could not read check-linux's and devices' make commands from check.yml")
    return got


DOCKERFILE = """FROM ubuntu:24.04
COPY ubuntu-setup.sh /tmp/ubuntu-setup.sh
RUN bash /tmp/ubuntu-setup.sh && rm -rf /var/lib/apt/lists/* /tmp/ubuntu-setup.sh \\
 && useradd -m -s /bin/bash ci
"""


def ubuntu_image(tree):
    """ubuntu:24.04 with tools/ubuntu-setup.sh run, built once a version of
    the script and kept"""
    setup = (tree / "tools/ubuntu-setup.sh").read_bytes()
    tag = "%s:%s" % (IMAGE, hashlib.sha256(setup + DOCKERFILE.encode()).hexdigest()[:12])
    if subprocess.run(["docker", "image", "inspect", tag], capture_output=True).returncode == 0:
        return tag
    print("merge train: building %s (ubuntu:24.04 and tools/ubuntu-setup.sh, once)" % tag, flush=True)
    with tempfile.TemporaryDirectory(prefix="zyperos-ubuntu-ci-") as ctx:
        (Path(ctx) / "ubuntu-setup.sh").write_bytes(setup)
        (Path(ctx) / "Dockerfile").write_text(DOCKERFILE)
        if subprocess.run(["docker", "build", "-q", "-t", tag, ctx]).returncode:
            raise Stop("could not build %s" % tag)
    return tag


def linux_half(tree, tmp, log):
    """The Linux jobs' commands in Ubuntu 24.04, on a copy of the tree
    (Docker Desktop's mounts are no place to build: the tree goes in
    read-only as a tar and is a git repository of its own in there, for
    the checks that ask git about it). Each command's (job, line, rc, s)."""
    tag = ubuntu_image(tree)
    (tmp / "in").mkdir(exist_ok=True)
    tar = tmp / "in" / "train.tar"
    with open(tar, "wb") as f:
        subprocess.run(["git", "archive", "--format=tar", "HEAD"], cwd=tree, stdout=f, check=True)
    jobs = os.environ.get("CHECK_JOBS") or "4"
    results = []
    for job, line in ci_commands(tree):
        script = ("set -e; mkdir -p /home/ci/work && cd /home/ci/work && tar -xf /in/train.tar && "
                  "git init -q && git add -A && git -c user.name=train -c user.email=train@localhost "
                  "commit -qm train && " + line)
        t0 = time.time()
        print("merge train: %s in %s: %s" % (job, tag, line if len(line) < 90 else line[:87] + "..."),
              flush=True)
        r = subprocess.run(["docker", "run", "--rm", "-u", "ci", "-e", "CHECK_JOBS=" + jobs,
                            "-v", "%s:/in:ro" % tar.parent, tag, "bash", "-c", script],
                           stdout=log, stderr=subprocess.STDOUT)
        results.append((job, line, r.returncode, time.time() - t0))
    tar.unlink()
    return results


def tail(path, n=60):
    try:
        return "".join(Path(path).read_text(errors="replace").splitlines(True)[-n:])
    except OSError:
        return ""


def gate(tree, tmp, name, docker):
    """make check here, then the Linux half; each part's (what, ok, s,
    log)"""
    parts = []
    slot = take_slot(name, "make check" + (", the Linux job and check-device-compile in Docker" if docker else ""))
    try:
        log = tmp / "make-check.log"
        print("merge train: make check (CHECK_JOBS=%s), log %s" % (os.environ.get("CHECK_JOBS") or "all", log),
              flush=True)
        t0 = time.time()
        with open(log, "w") as f:
            r = subprocess.run(["make", "check"], cwd=tree, stdout=f, stderr=subprocess.STDOUT)
        last = [l for l in tail(log, 400).splitlines() if " checks in " in l]
        parts.append(("make check (macOS)" + (": " + last[-1] if last else ""), r.returncode == 0,
                      time.time() - t0, log))
        print("merge train: make check %s in %.0f s" % ("passed" if r.returncode == 0 else "FAILED",
                                                       time.time() - t0), flush=True)
        if docker:
            if not shutil.which("docker") or subprocess.run(["docker", "info"], capture_output=True).returncode:
                parts.append(("the Linux job in Docker: Docker is not running (--no-docker leaves it out)",
                              False, 0.0, None))
            else:
                log = tmp / "linux.log"
                with open(log, "w") as f:
                    for job, line, rc, took in linux_half(tree, tmp, f):
                        parts.append(("%s in Ubuntu 24.04: %s" % (job, line.split(" CHECKS_SKIP")[0]),
                                      rc == 0, took, log))
                        print("merge train: %s %s in %.0f s" % (job, "passed" if rc == 0 else "FAILED", took),
                              flush=True)
    finally:
        give_slot(slot)
    return parts


# ---- what was found, kept --------------------------------------------------------

def record_path(base, prs):
    return KEPT / ("%s-%s" % (base[:12], "-".join(str(p["number"]) for p in prs)))


def passed_before(base, prs):
    """The kept record of a gate that passed on this main with these heads"""
    p = record_path(base, prs).with_suffix(".json")
    try:
        rec = json.loads(p.read_text())
    except (OSError, ValueError):
        return None
    heads = {str(pr["number"]): pr["headRefOid"] for pr in prs}
    if rec.get("base") == base and rec.get("heads") == heads and rec.get("verdict") == "pass":
        return rec
    return None


def keep(base, prs, name, tree_sha, notes, parts, verdict, merged=()):
    """The report (and, for a gate, its record: an integration alone never
    stands for a gate)"""
    KEPT.mkdir(parents=True, exist_ok=True)
    rec = {"base": base, "heads": {str(p["number"]): p["headRefOid"] for p in prs}, "name": name,
           "tree": tree_sha, "verdict": verdict, "when": time.strftime("%Y-%m-%d %H:%M"),
           "parts": [{"what": w, "ok": ok, "s": round(s)} for w, ok, s, _ in parts]}
    path = record_path(base, prs)
    if parts:
        path.with_suffix(".json").write_text(json.dumps(rec, indent=1))
    else:
        path = path.with_name(path.name + "-integration")
    lines = ["merge train %s, %s" % (name, rec["when"]), "main %s" % base[:12]]
    lines += notes
    lines.append("the train's tree %s" % tree_sha[:12])
    for w, ok, s, log in parts:
        lines.append("%s: %s in %.0f s%s" % (w, "passed" if ok else "FAILED", s, "" if ok or not log else
                                             " (log %s)" % log))
    lines.append("verdict: %s" % verdict)
    lines += list(merged)
    path.with_suffix(".txt").write_text("\n".join(lines) + "\n")
    return path.with_suffix(".txt")


# ---- merging on GitHub -----------------------------------------------------------

def plan_update(main, head, target, cwd=ROOT):
    """Whether head merged onto main lands target's tree as it is: None if
    it does, else the files that would come out otherwise (a conflict in a
    list, a generated file stale). Stop when any of those is neither a list
    nor generated: then main merged into the branch would carry the train's
    own change to it, which is a hand's."""
    r = git("merge-tree", "--write-tree", "--name-only", "--no-messages", main, head, cwd=cwd, check=False)
    out = r.stdout.split()
    natural = out[0] if out else ""
    if r.returncode == 0 and natural == target:
        return None
    differ = set(out[1:]) if r.returncode else set()
    if natural:
        differ |= set(git("diff", "--name-only", natural, target, cwd=cwd).stdout.split())
    stray = sorted(p for p in differ if p not in LISTS and not generated(p))
    if stray:
        raise Stop("merged on main it would differ from the train in %s" % ", ".join(stray))
    return sorted(differ)


def update_commit(main, head, target, branch, differ, cwd=ROOT):
    """main merged into a PR's branch, its tree the train's: the PR's head
    and main its parents, so the branch moves on and main merges it as it
    is"""
    return git("commit-tree", target, "-p", head, "-p", main, "-m",
               "Merge branch 'main' into %s\n\nThe check lists joined as tools/merge-train.py joined "
               "them (%s)." % (branch, ", ".join(differ)), cwd=cwd).stdout.strip()


def merge_on_github(prs, after, base_tree, repo):
    """Each PR merged on GitHub in order, each landing its train tree on
    main. A PR whose own merge would land another tree (its lists needed
    joining, or the generated files writing again) gets a merge of main into
    its branch first whose tree is the train's, pushed to the branch. Lines
    for the report."""
    said = []
    want = base_tree
    for pr, train in zip(prs, after):
        n = pr["number"]
        now = gh_json("pr", "view", str(n), "--json", "state,headRefOid,headRefName,headRepository")
        if now["state"] != "OPEN" or now["headRefOid"] != pr["headRefOid"]:
            raise Stop("#%d is %s at %s, not open at the head the gate took (%s): run again" % (
                n, now["state"].lower(), now["headRefOid"][:12], pr["headRefOid"][:12]))
        git("fetch", "-q", "origin", "+refs/heads/main:refs/remotes/origin/main")
        main = git("rev-parse", "origin/main").stdout.strip()
        if git("rev-parse", "origin/main^{tree}").stdout.strip() != want:
            raise Stop("main (%s) is not the tree the gate checked before #%d: it moved; run again" % (main[:12], n))
        target = git("rev-parse", train + "^{tree}").stdout.strip()
        head = pr["headRefOid"]
        try:
            differ = plan_update(main, head, target)
        except Stop as e:
            raise Stop("#%d %s: not merging it" % (n, e))
        if differ is not None:
            if (now.get("headRepository") or {}).get("name") != repo.split("/")[1] or \
                    pr.get("headRepositoryOwner", {}).get("login") != repo.split("/")[0]:
                raise Stop("#%d needs main merged into its branch (%s), which is in another repository: "
                           "merge it by hand" % (n, ", ".join(differ)))
            head = update_commit(main, head, target, pr["headRefName"], differ)
            git("-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential",
                "push", "-q", "origin", "%s:refs/heads/%s" % (head, pr["headRefName"]))
            print("merge train: #%d: main merged into %s (%s), pushed at %s" % (
                n, pr["headRefName"], ", ".join(differ), head[:12]), flush=True)
            said.append("#%d: main merged into its branch first (%s), at %s" % (n, ", ".join(differ), head[:12]))
        for attempt in range(10):
            r = subprocess.run(["gh", "pr", "merge", str(n), "--merge", "--match-head-commit", head],
                               cwd=ROOT, text=True, capture_output=True)
            if r.returncode == 0:
                break
            # GitHub works out a new head's mergeability a moment after the push
            if "not mergeable" not in (r.stderr + r.stdout).lower() and "head branch was modified" not in r.stderr:
                raise Stop("gh pr merge %d: %s" % (n, r.stderr.strip()))
            time.sleep(6)
        else:
            raise Stop("gh pr merge %d: %s" % (n, r.stderr.strip()))
        for attempt in range(20):
            git("fetch", "-q", "origin", "+refs/heads/main:refs/remotes/origin/main")
            if git("rev-parse", "origin/main").stdout.strip() != main:
                break
            time.sleep(3)
        got = git("rev-parse", "origin/main^{tree}").stdout.strip()
        if got != target:
            raise Stop("#%d merged, but main's tree (%s) is not the train's (%s): look before merging more"
                       % (n, got[:12], target[:12]))
        print("merge train: #%d merged on GitHub; main %s, the tree the gate checked" % (
            n, git("rev-parse", "origin/main").stdout.strip()[:12]), flush=True)
        said.append("#%d merged on GitHub (main's tree the train's)" % n)
        want = target
    return said


# ---- the train ------------------------------------------------------------------

def free_name(name):
    if git("rev-parse", "--verify", "-q", "refs/heads/" + name, check=False).returncode:
        return name
    for k in range(2, 100):
        if git("rev-parse", "--verify", "-q", "refs/heads/%s-%d" % (name, k), check=False).returncode:
            return "%s-%d" % (name, k)
    raise Stop("branches %s-2 to -99 are all taken: --name another" % name)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prs", nargs="+", type=int, help="pull request numbers, in merge order")
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument("--integrate-only", action="store_true",
                      help="the merges alone, the worktree kept and its path printed (a test card's tree)")
    mode.add_argument("--merge", action="store_true", help="after the gate, merge each PR on GitHub")
    mode.add_argument("--push", action="store_true", help="after the gate, push the train and open its pull request")
    ap.add_argument("--no-check", action="store_true", help="no gate")
    ap.add_argument("--no-docker", action="store_true", help="the gate without its Linux half")
    ap.add_argument("--recheck", action="store_true", help="the gate even if these heads passed it on this main")
    ap.add_argument("--keep", action="store_true", help="keep the worktree")
    ap.add_argument("--name", help="the branch (train-<numbers>)")
    ap.add_argument("--base", default="main", help="the branch the train is for (main)")
    args = ap.parse_args()
    if args.integrate_only:
        args.no_check = args.keep = True
    if args.merge and args.no_check:
        ap.error("--merge merges only what the gate passed: no --no-check")
    if args.merge and args.base != "main":
        ap.error("--merge is for main")

    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, stop)

    prs, tmp, tree, name, pushed, done = [], None, None, None, False, False
    try:
        name = free_name(args.name or "train-" + "-".join(str(n) for n in args.prs))
        repo = gh_json("repo", "view", "--json", "nameWithOwner")["nameWithOwner"]
        for n in args.prs:
            pr = gh_json("pr", "view", str(n), "--json",
                         "number,title,state,isDraft,baseRefName,headRefName,headRefOid,headRepositoryOwner")
            if pr["state"] != "OPEN":
                raise Stop("#%d is %s" % (n, pr["state"].lower()))
            if pr["baseRefName"] != args.base:
                raise Stop("#%d is for %s, not %s" % (n, pr["baseRefName"], args.base))
            if pr["isDraft"]:
                if args.merge:
                    raise Stop("#%d is a draft: mark it ready first" % n)
                print("merge train: #%d is a draft; it goes in anyway" % n)
            prs.append(pr)

        t0 = time.time()
        refs = ["+refs/heads/%s:refs/remotes/origin/%s" % (args.base, args.base)]
        refs += ["+refs/pull/%d/head:refs/train/%s/%d" % (pr["number"], name, pr["number"]) for pr in prs]
        print("merge train: fetching %s and %d heads" % (args.base, len(prs)), flush=True)
        git("fetch", "-q", "origin", *refs)
        for pr in prs:
            got = git("rev-parse", "refs/train/%s/%d" % (name, pr["number"])).stdout.strip()
            if got != pr["headRefOid"]:
                raise Stop("#%d moved while it was fetched; run again" % pr["number"])
        base = git("rev-parse", "origin/" + args.base).stdout.strip()

        tmp = Path(tempfile.mkdtemp(prefix="zyperos-train-"))
        tree = tmp / "train"
        git("worktree", "add", "-q", "-b", name, str(tree), base)
        after, notes = integrate(tree, prs, ["refs/train/%s/%d" % (name, pr["number"]) for pr in prs])
        head = git("rev-parse", "HEAD", cwd=tree).stdout.strip()
        tree_sha = git("rev-parse", "HEAD^{tree}", cwd=tree).stdout.strip()
        print("merge train: %d PRs merged in %.0f s; %s at %s" % (len(prs), time.time() - t0, name, head[:12]),
              flush=True)

        parts, verdict = [], "not checked (--no-check)"
        if not args.no_check:
            before = None if args.recheck else passed_before(base, prs)
            if before and before.get("tree") == tree_sha:
                print("merge train: these heads on this main passed the gate at %s; not run again "
                      "(--recheck runs it)" % before["when"], flush=True)
                parts = [(p["what"] + " (kept from %s)" % before["when"], p["ok"], p["s"], None)
                         for p in before["parts"]]
                verdict = "pass"
            else:
                parts = gate(tree, tmp, name, not args.no_docker)
                verdict = "pass" if all(ok for _, ok, _, _ in parts) else "FAIL"
        report = keep(base, prs, name, tree_sha, notes, parts, verdict)
        if verdict == "FAIL":
            args.keep = True
            for w, ok, _, log in parts:
                if not ok and log:
                    print("\n=== %s (%s) ===\n%s" % (w, log, tail(log)), file=sys.stderr)
            print("merge train: the gate FAILED; the train is in %s; report %s" % (tree, report), file=sys.stderr)
            return 1
        if parts:
            print("merge train: the gate passed: " + "; ".join("%s %.0f s" % (w, s) for w, _, s, _ in parts))

        if args.merge:
            merged = merge_on_github(prs, after, git("rev-parse", base + "^{tree}").stdout.strip(), repo)
            report = keep(base, prs, name, tree_sha, notes, parts, verdict, merged)
            print("merge train: all %d merged; report %s" % (len(prs), report))
        elif args.push:
            git("-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential",
                "push", "-q", "origin", "HEAD:refs/heads/" + name, cwd=tree, capture=False)
            pushed = True
            body = ["Merges these pull requests onto `%s` together, in this order:" % args.base, ""]
            body += ["- #%d %s (head `%s`)" % (p["number"], p["title"], p["headRefOid"][:12]) for p in prs]
            body += ["", "Built with `tools/merge-train.py`: %s." % (
                "; ".join("%s %s" % (w, "passed" if ok else "failed") for w, ok, _, _ in parts) or verdict), "",
                "Merge it with a merge commit (`gh pr merge --merge`), not a squash or a rebase: "
                "then each pull request's own commits are in `%s`, and GitHub marks each one "
                "merged. A PR that got a new commit after this train was built is not in it." % args.base]
            title = "Merge train: " + ", ".join("#%d" % p["number"] for p in prs)
            r = subprocess.run(["gh", "pr", "create", "--base", args.base, "--head", name, "--title", title,
                                "--body", "\n".join(body)], cwd=ROOT, text=True, stdout=subprocess.PIPE)
            print("merge train: pushed %s at %s; its pull request: %s" % (name, head[:12], r.stdout.strip()))
        elif args.integrate_only:
            print("merge train: the integration is %s (branch %s at %s); report %s" % (tree, name, head[:12], report))
            print(tree)
        else:
            print("merge train: %s at %s, here only; report %s. --merge merges the PRs, --push opens "
                  "the train's own pull request" % (name, head[:12], report))
        done = True
        return 0
    except Stop as e:
        print("merge train: %s" % e, file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("merge train: stopped", file=sys.stderr)
        return 130
    finally:
        for pr in prs:
            git("update-ref", "-d", "refs/train/%s/%d" % (name, pr["number"]), check=False)
        if tree is not None:
            if args.keep:
                if not args.integrate_only:
                    print("merge train: the worktree is %s (git worktree remove %s when done)" % (tree, tree))
            else:
                git("worktree", "remove", "--force", str(tree), check=False)
                shutil.rmtree(tmp, ignore_errors=True)
                # Pushed, the branch is the remote's; failed, it is nothing
                if pushed or not done or args.merge:
                    git("branch", "-q", "-D", name, check=False)


if __name__ == "__main__":
    sys.exit(main())
