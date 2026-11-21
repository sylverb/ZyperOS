#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tools/merge-train.py's merges, in a scratch repository of its own (no
GitHub, no network):

- two branches that each add a check to the Makefile's .PHONY and CHECKS
  lists and a row to docs/building.md's table, side by side as every such
  PR does, merge one after the other with both checks in both lists and
  both rows kept, each list's other names as they were
- a name added in the middle of a list, and one at a list's start
- a row added by one PR next to a row the other rewrote: both, the row as
  rewritten (the merge base says which side changed it)
- refused, the train stopped and the file named: a conflict in a rule's
  body made of plain words (not a list, though it looks like one), the same
  row written two ways, prose beside the rows, and a conflict in any other
  file
- after the merges, the table written again by tools/building-doc.py (a
  stand-in here that puts the rows in the names' order), in a commit of
  its own
- --merge's step on GitHub, played here: the second branch merged onto a
  main that has the first, as GitHub merges, would not land the train's
  tree; main merged into the branch with the train's tree does, and a
  difference outside the lists is refused

make check-mergetrain.
"""
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parent.parent
fails = []


def check(ok, what):
    print(('ok   ' if ok else 'FAIL ') + what)
    if not ok:
        fails.append(what)


spec = importlib.util.spec_from_file_location('merge_train', ROOT / 'tools' / 'merge-train.py')
mt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mt)

MAKEFILE = """.PHONY: all clean check check-one check-two \\
        check-three check-four

# The lists are far apart in the real Makefile: so they are here, or git
# makes one hunk of both, which (holding a rule) is not joined
all: check

clean:
\t@true

CHECKS = check-one check-two \\
         check-three check-four

check-one:
\t@true
check-two:
\t@echo two
"""

DOC = """# Building

| | |
|---|---|
| `check-one` | the first |
| `check-two` | the second |
| `check-three` | the third |
| `check-four` | the fourth |

Prose after the table.
"""


# tools/building-doc.py's part here: the rows in the names' order
BUILDING_DOC = """import re
p = 'docs/building.md'
lines = open(p).read().split('\\n')
idx = [i for i, l in enumerate(lines) if l.startswith('| `')]
rows = sorted(lines[i] for i in idx)
for i, r in zip(idx, rows):
    lines[i] = r
open(p, 'w').write('\\n'.join(lines))
"""


def sh(cwd, *args):
    r = subprocess.run(args, cwd=cwd, text=True, capture_output=True)
    if r.returncode:
        raise SystemExit('%s: %s' % (' '.join(args), r.stderr))
    return r.stdout.strip()


def repo(tmp):
    d = tmp / 'repo'
    d.mkdir()
    sh(d, 'git', 'init', '-q', '-b', 'main')
    sh(d, 'git', 'config', 'user.name', 'train')
    sh(d, 'git', 'config', 'user.email', 'train@localhost')
    (d / 'docs').mkdir()
    (d / 'src').mkdir()
    (d / 'Makefile').write_text(MAKEFILE)
    (d / 'docs/building.md').write_text(DOC)
    (d / 'src/a.c').write_text('int a = 1;\n')
    (d / 'tools').mkdir()
    (d / 'tools/building-doc.py').write_text(BUILDING_DOC)
    sh(d, 'git', 'add', '-A')
    sh(d, 'git', 'commit', '-qm', 'base')
    return d


def branch(d, name, edits, base='main'):
    sh(d, 'git', 'checkout', '-q', '-b', name, base)
    for path, (old, new) in edits:
        p = d / path
        text = p.read_text()
        assert old in text, (path, old)
        p.write_text(text.replace(old, new, 1))
    sh(d, 'git', 'commit', '-qam', name)
    sh(d, 'git', 'checkout', '-q', 'main')
    return sh(d, 'git', 'rev-parse', name)


def adds(name, row, after):
    # Its rule beside another's, as a PR puts one: only the lists are side
    # by side (a rule at the same place in both is a hand's)
    return [
        ('Makefile', ('check-three check-four\n\n# The', 'check-three check-four %s\n\n# The' % name)),
        ('Makefile', ('         check-three check-four\n\n', '         check-three check-four %s\n\n' % name)),
        ('Makefile', (after, after + '%s:\n\t@true\n' % name)),
        ('docs/building.md', ('| `check-four` | the fourth |\n', '| `check-four` | the fourth |\n| `%s` | %s |\n'
                              % (name, row))),
    ]


def pr(n, ref):
    return {'number': n, 'title': 'pr %d' % n, 'headRefName': 'b%d' % n, 'headRefOid': ref,
            'headRepositoryOwner': {'login': 'me'}}


def train(d, tmp, refs, name):
    tree = tmp / name
    sh(d, 'git', 'worktree', 'add', '-q', '-b', name, str(tree), 'main')
    prs = [pr(k + 1, r) for k, r in enumerate(refs)]
    try:
        after, notes = mt.integrate(tree, prs, refs)
        return tree, after, None
    except mt.Stop as e:
        return tree, None, str(e)


def lists(text):
    return mt.make_lists(text)


def main():
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-mergetrain-'))
    try:
        d = repo(tmp)
        a = branch(d, 'alpha', adds('check-alpha', 'alpha\'s row', 'check-one:\n\t@true\n'))
        b = branch(d, 'beta', adds('check-beta', 'beta\'s row', 'check-two:\n\t@echo two\n'))
        tree, after, err = train(d, tmp, [a, b], 'train-ab')
        check(err is None, f'two PRs that each add a check merge: {err}')
        if err is None:
            mk = (tree / 'Makefile').read_text()
            got = lists(mk)
            want = {'check-one', 'check-two', 'check-three', 'check-four', 'check-alpha', 'check-beta'}
            check(got['checks'] == want, f'CHECKS has both, and the rest: {sorted(got["checks"])}')
            check(got['phony'] == want | {'all', 'clean', 'check'}, f'.PHONY has both, and the rest')
            check('check-alpha:\n' in mk and 'check-beta:\n' in mk, 'both rules are there')
            check('<<<<<<<' not in mk and '>>>>>>>' not in mk, 'no conflict marker left in the Makefile')
            doc = (tree / 'docs/building.md').read_text()
            check('| `check-alpha` | alpha\'s row |' in doc and '| `check-beta` | beta\'s row |' in doc,
                  'docs/building.md keeps both rows')
            check(doc.count('| `check-four` |') == 1 and 'Prose after the table.' in doc and '<<<<<<<' not in doc,
                  'and every other line once')
            names = re.findall(r'^\| `([\w-]+)`', doc, re.M)
            check(names == sorted(names), f'and the rows in the names\' order after the merges ({names})')
            log = sh(tree, 'git', 'log', '--format=%s', '-3').splitlines()
            check(log[0].startswith('Write the generated files again') and log[1].startswith('Merge pull request #2')
                  and log[2].startswith('Merge pull request #1'),
                  f'each PR a merge commit of its own, the table written again in one after them ({log})')

            # GitHub's half of --merge: main with alpha merged, then beta
            sh(d, 'git', 'checkout', '-q', 'main')
            sh(d, 'git', 'merge', '-q', '--no-ff', '-m', 'Merge pull request #1', a)
            main1 = sh(d, 'git', 'rev-parse', 'main')
            t1 = sh(d, 'git', 'rev-parse', after[0] + '^{tree}')
            check(sh(d, 'git', 'rev-parse', 'main^{tree}') == t1, 'the first PR merged on main is the train\'s tree')
            target = sh(d, 'git', 'rev-parse', after[1] + '^{tree}')
            differ = mt.plan_update(main1, b, target, cwd=d)
            check(differ == ['Makefile', 'docs/building.md'],
                  f'the second, merged as it is, would not land the train\'s tree: {differ}')
            u = mt.update_commit(main1, b, target, 'beta', differ, cwd=d)
            check(sh(d, 'git', 'rev-parse', u + '^1') == b and sh(d, 'git', 'rev-parse', u + '^2') == main1,
                  'main merged into the branch: its head first, then main')
            check(mt.plan_update(main1, u, target, cwd=d) is None,
                  'with main merged into it, it lands the train\'s tree')
            stray = branch(d, 'stray', [('src/a.c', ('int a = 1;', 'int a = 2;'))])
            try:
                mt.plan_update(main1, stray, target, cwd=d)
                check(False, 'a difference outside the lists is refused')
            except mt.Stop as e:
                check('src/a.c' in str(e), f'a difference outside the lists is refused ({e})')

        # The rest from the first commit again
        sh(d, 'git', 'checkout', '-q', 'main')
        sh(d, 'git', 'reset', '-q', '--hard', sh(d, 'git', 'rev-list', '--max-parents=0', 'main'))

        # A name in the middle and one at the start of a list
        mid = branch(d, 'mid', [('Makefile', ('CHECKS = check-one check-two', 'CHECKS = check-one check-mid check-two'))],
                     base=sh(d, 'git', 'rev-list', '--max-parents=0', 'main'))
        first = branch(d, 'first', [('Makefile', ('CHECKS = check-one', 'CHECKS = check-first check-one'))],
                       base=sh(d, 'git', 'rev-list', '--max-parents=0', 'main'))
        tree, _, err = train(d, tmp, [mid, first], 'train-mf')
        got = lists((tree / 'Makefile').read_text())['checks'] if err is None else set()
        check(err is None and got == {'check-mid', 'check-first', 'check-one', 'check-two', 'check-three', 'check-four'},
              f'a name in the middle and one at the start: both kept ({err or sorted(got)})')

        # A row added beside one the other PR rewrote (#426 and #430,
        # 2026-11-21: check-pico8-quit's row before check-pico8-mp4's, whose
        # words #430 changed)
        root = sh(d, 'git', 'rev-list', '--max-parents=0', 'main')
        added = branch(d, 'added', [('docs/building.md', ('| `check-two` |', '| `check-new` | a new one |\n| `check-two` |'))],
                       base=root)
        edited = branch(d, 'edited', [('docs/building.md', ('the second |', 'the second, said better |'))], base=root)
        tree, _, err = train(d, tmp, [added, edited], 'train-ae')
        doc = (tree / 'docs/building.md').read_text() if err is None else ''
        check(err is None and '| `check-new` | a new one |' in doc and '| `check-two` | the second, said better |' in doc
              and 'the second |' not in doc,
              f'a row added beside one the other rewrote: both, the row as rewritten ({err})')

        # Refused
        cases = [
            ('a rule\'s body of plain words', 'Makefile',
             [('Makefile', ('\t@true\ncheck-two', '\ttrue one\ncheck-two'))],
             [('Makefile', ('\t@true\ncheck-two', '\ttrue two\ncheck-two'))]),
            ('the same row written two ways', 'docs/building.md',
             [('docs/building.md', ('the second |', 'the second, one way |'))],
             [('docs/building.md', ('the second |', 'the second, another |'))]),
            ('prose beside the rows', 'docs/building.md',
             [('docs/building.md', ('Prose after the table.', 'Prose, one way.'))],
             [('docs/building.md', ('Prose after the table.', 'Prose, another.'))]),
            ('any other file', 'src/a.c',
             [('src/a.c', ('int a = 1;', 'int a = 3;'))],
             [('src/a.c', ('int a = 1;', 'int a = 4;'))]),
        ]
        for k, (what, path, one, two) in enumerate(cases):
            x = branch(d, 'x%d' % k, one, base=root)
            y = branch(d, 'y%d' % k, two, base=root)
            tree, _, err = train(d, tmp, [x, y], 'train-x%d' % k)
            check(err is not None and path in err and '#2' in err,
                  f'refused: {what}, the train stopped at the second PR, {path} named ({err})')
            status = sh(tree, 'git', 'status', '--porcelain')
            check(status == '', f'and the train left as it was before that PR ({status!r})')
    finally:
        subprocess.call(['rm', '-rf', str(tmp)])
    print(f'check-mergetrain: {"all passed" if not fails else f"{len(fails)} FAILED"}')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
