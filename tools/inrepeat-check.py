#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""check-inrepeat: no button read twice by in_repeat in one step (src/platform.h).

A repeat is true once: in_repeat moves the button's repeat clock when it says
yes, so a second read of a held button in the same frame says no. A step
written as "either is due, then which one" read it twice:

    if (in_repeat(&a->in, IN_LEFT) || in_repeat(&a->in, IN_RIGHT)) {
        int dir = in_repeat(&a->in, IN_RIGHT) ? 1 : -1;

and on every repeat of a held Right the second read was false and the step
went left. The home's Up and Down did it (on the 1.1.0 test build, 2026-11-19),
the in-game save slots, File Transfer's question and the USB questions. Each
button is read once now (in_repeat_axis). This holds every source file in
src/ to it: the same in_repeat(state, button) twice within four lines fails.
A reading of the source; no SDL."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WINDOW = 3          # lines after a read in which a second read of it fails


def code(text):
    """The text with comments and strings blanked, its line breaks kept."""
    blank = lambda m: re.sub(r'[^\n]', ' ', m.group())
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"', blank, text, flags=re.S)


READ = re.compile(r'\bin_repeat\s*\(\s*([^,()]+?)\s*,\s*(\w+)\s*\)')


def twice(text):
    """(line, line, state, button) for each button read twice in a step"""
    reads = [(i, m.group(1).replace(' ', ''), m.group(2))
             for i, line in enumerate(code(text).split('\n')) for m in READ.finditer(line)]
    out = []
    for a in range(len(reads)):
        for b in range(a + 1, len(reads)):
            if reads[b][0] - reads[a][0] > WINDOW:
                break
            if reads[a][1:] == reads[b][1:]:
                out.append((reads[a][0] + 1, reads[b][0] + 1) + reads[a][1:])
    return out


fails = 0
# The rule reads what it is meant to: the shape that turned a held Right left
BAD = '''if (in_repeat(&a->in, IN_LEFT) || in_repeat(&a->in, IN_RIGHT)) {
	int dir = in_repeat(&a->in, IN_RIGHT) ? 1 : -1, next = sel;
}'''
GOOD = '''int dir = in_repeat_axis(&a->in, IN_LEFT, IN_RIGHT);
if (in_repeat(&a->in, IN_L1)) page(-1);
if (in_repeat(&a->in, IN_R1)) page(+1);'''
if not twice(BAD):
    print('FAIL the rule does not see a held button read twice in "either, then which"')
    fails += 1
if twice(GOOD):
    print('FAIL the rule takes buttons read once each for a double read')
    fails += 1

files = reads = 0
for path in sorted((ROOT / 'src').rglob('*.c')):
    text = path.read_text()
    files += 1
    reads += len(READ.findall(code(text)))
    for l1, l2, state, button in twice(text):
        print(f'FAIL {path.relative_to(ROOT)}:{l1} and {l2}: in_repeat({state}, {button}) read twice in one '
              f'step; the second read of a held button is false (use in_repeat_axis, or read it once)')
        fails += 1
# A floor that only proves the pattern still finds the reads: the volume and
# brightness keys moved into one levels_keys (src/levels.c) and took the
# screens' own reads with them, 88 left in 2026-11; a broken pattern finds few
if reads < 50:
    print(f'FAIL only {reads} in_repeat reads found in src/: the check is not reading them')
    fails += 1
if fails:
    sys.exit(1)
print(f'ok: {reads} in_repeat reads in {files} files, no button read twice in a step')
