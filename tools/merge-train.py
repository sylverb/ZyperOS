#!/usr/bin/env python3
"""A merge train: several pull requests merged onto main at once, checked once.

    python3 tools/merge-train.py 281 283 284
                    merges each PR's head onto origin/main, in that order, in
                    a scratch worktree of its own; writes again what is
                    generated (tools/i18n.py, tools/designdef.py); runs one
                    make check; keeps the branch train-281-283-284 here
    python3 tools/merge-train.py --push 281 283 284
                    and pushes the branch and opens its pull request, which
                    merged with a merge commit lands the PRs together
    --no-check      the merges and the generated files, no make check
    --keep          the worktree stays (its path is printed)
    --name NAME     the train's branch (train-<numbers>)

Merged one at a time, every merge to main sent each other open pull request
back to merge main, write the strings again and run the whole make check
(twenty minutes and more on a loaded Mac), and the generated files they all
carry conflicted after each merge. The train merges them in one go: a
conflict in a file a generator writes whole (src/i18n_ids.h, res/web/i18n/,
src/design.h ...) is settled by writing the file again from the merged sources; a
conflict anywhere else stops the train at that PR, which then needs main
merged in by hand. Nothing is pushed without --push, and main is never
pushed: the train is a pull request of its own. Merged with a merge commit
(gh pr merge N --merge; not a squash or a rebase), each PR's commits are in
main and GitHub marks each PR merged.

CHECK_JOBS goes to make check as it is (one check at a time with 1). Needs
git and, for the PRs and --push, gh.
"""
import argparse
import json
import os
from pathlib import Path
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


def git(*args, cwd=ROOT, check=True, capture=True):
    r = subprocess.run(["git", *args], cwd=cwd, text=True,
                       stdout=subprocess.PIPE if capture else None,
                       stderr=subprocess.PIPE if capture else None)
    if check and r.returncode:
        sys.exit("merge train: git %s: %s" % (" ".join(args), (r.stderr or "").strip()))
    return r


def gh_json(*args):
    r = subprocess.run(["gh", *args], cwd=ROOT, text=True, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
    if r.returncode:
        sys.exit("merge train: gh %s: %s" % (" ".join(args), r.stderr.strip()))
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
                sys.exit("merge train: %s failed" % " ".join(gen))


def ignored(tree, path):
    """Whether the train's .gitignore keeps path out (src/i18n_tables.c,
    since the build writes it)"""
    return git("check-ignore", "-q", "--no-index", "--", path, cwd=tree, check=False).returncode == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("prs", nargs="+", type=int, help="pull request numbers, in merge order")
    ap.add_argument("--push", action="store_true", help="push the train and open its pull request")
    ap.add_argument("--no-check", action="store_true", help="no make check")
    ap.add_argument("--keep", action="store_true", help="keep the worktree")
    ap.add_argument("--name", help="the branch (train-<numbers>)")
    ap.add_argument("--base", default="main", help="the branch the train is for (main)")
    args = ap.parse_args()

    def stop(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, stop)

    name = args.name or "train-" + "-".join(str(n) for n in args.prs)
    if git("rev-parse", "--verify", "-q", "refs/heads/" + name, check=False).returncode == 0:
        sys.exit("merge train: a branch %s is here already: --name another, or delete it" % name)

    prs = []
    for n in args.prs:
        pr = gh_json("pr", "view", str(n), "--json",
                     "number,title,state,isDraft,baseRefName,headRefName,headRefOid,headRepositoryOwner")
        if pr["state"] != "OPEN":
            sys.exit("merge train: #%d is %s" % (n, pr["state"].lower()))
        if pr["baseRefName"] != args.base:
            sys.exit("merge train: #%d is for %s, not %s" % (n, pr["baseRefName"], args.base))
        if pr["isDraft"]:
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
            sys.exit("merge train: #%d moved while it was fetched; run again" % pr["number"])

    tmp = Path(tempfile.mkdtemp(prefix="zyperos-train-"))
    tree = tmp / "train"
    pushed = done = False
    try:
        git("worktree", "add", "-q", "-b", name, str(tree), "origin/" + args.base)
        for pr in prs:
            n = pr["number"]
            owner = (pr.get("headRepositoryOwner") or {}).get("login", "")
            message = "Merge pull request #%d from %s/%s" % (n, owner, pr["headRefName"])
            r = git("merge", "--no-ff", "-m", message, "refs/train/%s/%d" % (name, n), cwd=tree, check=False)
            if r.returncode == 0:
                print("merge train: #%d %s: merged" % (n, pr["title"]), flush=True)
                continue
            conflicted = git("diff", "--name-only", "--diff-filter=U", cwd=tree).stdout.split()
            if not conflicted:
                git("merge", "--abort", cwd=tree, check=False)
                sys.exit("merge train: #%d did not merge: %s" % (n, r.stderr.strip() or r.stdout.strip()))
            hand = [p for p in conflicted if not generated(p)]
            if hand:
                git("merge", "--abort", cwd=tree, check=False)
                print("merge train: #%d conflicts with the train in %s: merge main (and the PRs "
                      "before it here) into it by hand, or leave it out" % (n, ", ".join(hand)),
                      file=sys.stderr)
                return 1
            # Only generated files: each is written again from the merged
            # sources. The string tables are no longer committed: where the
            # train's main ignores them, a PR from before that carries them
            # loses them here.
            gens = []
            for p in conflicted:
                gen = generated(p)
                if gen not in gens:
                    gens.append(gen)
                if ignored(tree, p):
                    git("rm", "-q", "--cached", "--ignore-unmatch", "--", p, cwd=tree)
                else:
                    git("checkout", "--ours", "--", p, cwd=tree, check=False)
            regenerate(tree, gens)
            for p in conflicted:
                if (tree / p).exists() and not ignored(tree, p):
                    git("add", "--", p, cwd=tree)
            git("commit", "-q", "--no-edit", cwd=tree)
            print("merge train: #%d %s: merged, %s written again" % (
                n, pr["title"], ", ".join(conflicted)), flush=True)

        # Whatever merged cleanly but is stale now: two PRs that each added a
        # string merge without a conflict and leave the IDs a step behind
        regenerate(tree, list(GENERATED))
        outs = [o for outs in GENERATED.values() for o in outs
                if (tree / o).exists() and not ignored(tree, o.rstrip("/"))]
        git("add", "-A", "--", *outs, cwd=tree)
        if git("diff", "--cached", "--quiet", cwd=tree, check=False).returncode:
            git("commit", "-q", "-m", "Write the generated files again after the train's merges", cwd=tree)
            print("merge train: the generated files written again (a commit of their own)")
        merged = time.time()
        print("merge train: %d PRs merged in %.0f s" % (len(prs), merged - t0), flush=True)

        verdict = "make check was not run (--no-check)"
        if not args.no_check:
            print("merge train: make check (CHECK_JOBS=%s)" % (os.environ.get("CHECK_JOBS") or "all"),
                  flush=True)
            r = subprocess.run(["make", "check"], cwd=tree)
            took = time.time() - merged
            if r.returncode:
                args.keep = True
                print("merge train: make check failed after %.0f s; the train is in %s "
                      "(build-native/check-logs)" % (took, tree), file=sys.stderr)
                return 1
            verdict = "make check passed on the train (%.0f s)" % took
            print("merge train: " + verdict)

        head = git("rev-parse", "HEAD", cwd=tree).stdout.strip()
        if args.push:
            git("-c", "credential.helper=", "-c", "credential.helper=!gh auth git-credential",
                "push", "-q", "origin", "HEAD:refs/heads/" + name, cwd=tree, capture=False)
            pushed = True
            body = ["Merges these pull requests onto `%s` together, in this order:" % args.base, ""]
            body += ["- #%d %s (head `%s`)" % (p["number"], p["title"], p["headRefOid"][:12]) for p in prs]
            body += ["", "Built with `tools/merge-train.py`: %s." % verdict, "",
                     "Merge it with a merge commit (`gh pr merge --merge`), not a squash or a rebase: "
                     "then each pull request's own commits are in `%s`, and GitHub marks each one "
                     "merged. A PR that got a new commit after this train was built is not in it." % args.base]
            title = "Merge train: " + ", ".join("#%d" % p["number"] for p in prs)
            r = subprocess.run(["gh", "pr", "create", "--base", args.base, "--head", name, "--title", title,
                                "--body", "\n".join(body)], cwd=ROOT, text=True, stdout=subprocess.PIPE)
            print("merge train: pushed %s at %s; its pull request: %s" % (name, head[:12], r.stdout.strip()))
        else:
            print("merge train: %s at %s, here only. Push it with --push, or: git push origin %s" % (
                name, head[:12], name))
        done = True
        return 0
    except KeyboardInterrupt:
        print("merge train: stopped", file=sys.stderr)
        return 130
    finally:
        for pr in prs:
            git("update-ref", "-d", "refs/train/%s/%d" % (name, pr["number"]), check=False)
        if args.keep:
            print("merge train: the worktree is %s (git worktree remove %s when done)" % (tree, tree))
        else:
            git("worktree", "remove", "--force", str(tree), check=False)
            shutil.rmtree(tmp, ignore_errors=True)
            # Pushed, the branch is the remote's; failed, it is nothing
            if pushed or not done:
                git("branch", "-q", "-D", name, check=False)


if __name__ == "__main__":
    sys.exit(main())
