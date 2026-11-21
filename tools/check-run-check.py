#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""make check's runner itself (tools/check-run.py), with stand-in checks:

- --shard I/N: the N parts together are the list, each check in one part
  but the SPLIT ones, in every part; the same parts from the list in any
  order; no part more than the longest check over another; a check the
  table has not met counted as NEW_CHECK
- the heavy slots: never more heavy checks at once than slots, in one run or
  across two; the rest go on while the heavy ones wait for one; a slot held
  by another process is waited for and taken once it ends, however it ends
  (a lock, not a folder left behind); and the time a check waited said on
  its line
- the slots' folder: the user's own cache, not /tmp; one that another user
  could write (mode 777) refused, and the run with it; a slot that is a link
  refused, the file it points to left as it was

make check-checkrun. No make, no launcher: the stand-in check is this file.
"""
import importlib.util
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
fails = []


def check(ok, what):
    print(('ok   ' if ok else 'FAIL ') + what)
    if not ok:
        fails.append(what)


def load():
    spec = importlib.util.spec_from_file_location('check_run', ROOT / 'tools' / 'check-run.py')
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def shards(cr):
    rnd = random.Random(7)
    times = {f'check-{k}': rnd.choice([0.2, 1, 3, 8, 20, 45, 110, 570]) for k in range(240)}
    checks = list(times) + ['check-new-a', 'check-new-b', 'check-i18n-shots']
    times['check-i18n-shots'] = 1546
    for n in (1, 2, 3, 4, 7):
        parts = [cr.shard(checks, i, n, times) for i in range(1, n + 1)]
        seen = [c for p in parts for c in p if c not in cr.SPLIT]
        check(sorted(seen) == sorted(c for c in checks if c not in cr.SPLIT),
              f'{n} parts: every check in exactly one')
        check(all('check-i18n-shots' in p for p in parts), f'{n} parts: check-i18n-shots in each')
        check(all(p == [c for c in checks if c in p] for p in parts), f'{n} parts: each in the list\'s order')
        shuffled = checks[:]
        rnd.shuffle(shuffled)
        again = [sorted(cr.shard(shuffled, i, n, times)) for i in range(1, n + 1)]
        check(again == [sorted(p) for p in parts], f'{n} parts: the same parts from the list in another order')
        load = [sum(times.get(c, cr.NEW_CHECK) for c in p if c not in cr.SPLIT) for p in parts]
        longest = max(t for c, t in times.items() if c not in cr.SPLIT)
        check(max(load) - min(load) <= longest, f'{n} parts: within the longest check of each other '
              f'({min(load):.0f} to {max(load):.0f} s)')
    one = cr.shard(['check-a', 'check-b'], 1, 2, {})
    two = cr.shard(['check-a', 'check-b'], 2, 2, {})
    check(len(one) == 1 and len(two) == 1, 'two checks the table has not met: one a part')


STAND_IN = r'''
import os, sys, time
name = sys.argv[1]
with open(os.environ['CHECKRUN_TRACE'], 'a') as f:
    f.write('start %s %.4f\n' % (name, time.time()))
time.sleep(0.4 if name.startswith('heavy') else 0.15)
with open(os.environ['CHECKRUN_TRACE'], 'a') as f:
    f.write('end %s %.4f\n' % (name, time.time()))
sys.exit(1 if name == 'light-fails' else 0)
'''

HOLD = r'''
import fcntl, sys, time
f = open(sys.argv[1], 'a+')
fcntl.flock(f, fcntl.LOCK_EX)
f.seek(0); f.truncate(); f.write('the other run\n'); f.flush()
print('held', flush=True)
time.sleep(float(sys.argv[2]))
'''


def spans(trace):
    start, end = {}, {}
    for line in trace.read_text().splitlines():
        kind, name, t = line.split()
        (start if kind == 'start' else end)[name] = float(t)
    return {n: (start[n], end[n]) for n in start}


def most_at_once(sp, names):
    edges = sorted([(sp[n][0], 1) for n in names] + [(sp[n][1], -1) for n in names])
    now = top = 0
    for _, d in edges:
        now += d
        top = max(top, now)
    return top


def slots(cr):
    tmp = Path(tempfile.mkdtemp(prefix='zyperos-checkrun-'))
    stand_in = tmp / 'stand-in.py'
    stand_in.write_text(STAND_IN)
    trace = tmp / 'trace'
    cr.LOGS, cr.TIMES = tmp / 'logs', tmp / 'logs' / 'times.tsv'
    cr.SLOTS_DIR = tmp / 'slots'
    cr.MAKE = [sys.executable, str(stand_in)]
    cr.HEAVY = {'heavy-1', 'heavy-2', 'heavy-3', 'heavy-4'}
    cr.DOCKER = {}
    os.environ.pop('CI', None)
    os.environ['CHECKRUN_TRACE'] = str(trace)
    heavy = sorted(cr.HEAVY)
    light = ['light-1', 'light-2', 'light-3', 'light-4', 'light-5', 'light-6']

    # One slot, four at a time: one heavy one at once, the light ones beside
    os.environ['CHECK_HEAVY_SLOTS'] = '1'
    rc = cr.main(['-j', '4'] + heavy + light)
    sp = spans(trace)
    check(rc == 0 and len(sp) == 10, f'one slot: every check ran and passed ({len(sp)} of 10)')
    check(most_at_once(sp, heavy) == 1, f'one slot: one heavy check at a time ({most_at_once(sp, heavy)})')
    check(most_at_once(sp, heavy + light) >= 3, 'one slot: the light ones run beside it')
    check(max(sp[n][1] for n in light) < max(sp[n][1] for n in heavy),
          'one slot: the light ones done before the heavy ones, which waited')

    # Two slots: two at once, never three
    trace.unlink()
    os.environ['CHECK_HEAVY_SLOTS'] = '2'
    rc = cr.main(['-j', '6'] + heavy + light)
    sp = spans(trace)
    check(rc == 0 and most_at_once(sp, heavy) == 2, f'two slots: two heavy checks at a time '
          f'({most_at_once(sp, heavy)})')

    # Another run holds the only slot: the heavy ones wait for it, the light
    # ones do not, and the slot is taken once the other run is killed
    trace.unlink()
    os.environ['CHECK_HEAVY_SLOTS'] = '1'
    cr.SLOTS_DIR.mkdir(exist_ok=True)
    other = subprocess.Popen([sys.executable, '-c', HOLD, str(cr.SLOTS_DIR / 'slot-1'), '30'],
                             stdout=subprocess.PIPE, text=True)
    check(other.stdout.readline().strip() == 'held', 'the other run holds the slot')
    held = cr.Slots(1).take('mine')
    check(held is None, 'a slot another process holds is not taken')
    check(cr.Slots(1).holders() == ['the other run'], 'the slot says who holds it')
    t_kill = time.time() + 1.5

    def killer():
        time.sleep(1.5)
        other.kill()
    import threading
    threading.Thread(target=killer, daemon=True).start()
    import contextlib
    import io
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        rc = cr.main(['-j', '3', 'heavy-1', 'light-1', 'light-2', 'light-fails'])
    other.wait()
    said = [line for line in out.getvalue().splitlines() if ' heavy-1 ' in line]
    check(any('(waited ' in line for line in said), f'its line says how long it waited: {said}')
    check(any('waiting for a heavy slot' in line and 'the other run' in line for line in said),
          'and while it waits, for whom')
    sp = spans(trace)
    check(rc == 1, 'a failing check still fails the run')
    check(sp['heavy-1'][0] >= t_kill - 0.05, f'the heavy one started after the other run ended '
          f'({sp["heavy-1"][0] - t_kill:+.2f} s)')
    check(all(sp[n][1] < t_kill for n in ('light-1', 'light-2')), 'the light ones did not wait for it')
    times = cr.read_times(cr.TIMES)
    check({'heavy-1', 'light-6'} <= set(times), 'times.tsv keeps the checks a smaller run did not run')
    got = cr.Slots(1).take('mine')
    check(got not in (None, True), 'the slot is free again after the run')
    cr.Slots(1).give(got)
    check(cr.Slots(1).holders() == [], 'a slot let go says it is free')

    # Planted: a folder anyone writes, a slot that is a link
    where = str(load().SLOTS_DIR)
    check(not where.startswith(('/tmp', '/private/tmp', '/var/tmp')),
          f'the slots are in the user\'s own cache, not a shared temp folder ({where})')
    open_dir = tmp / 'open'
    open_dir.mkdir()
    os.chmod(open_dir, 0o777)
    try:
        cr.Slots(1, open_dir).take('mine')
        check(False, 'a slots folder anyone writes is refused')
    except cr.Refused as e:
        check('mode' in str(e), f'a slots folder anyone writes is refused ({e})')
    cr.SLOTS_DIR = open_dir
    trace.write_text('')
    rc = cr.main(['-j', '2', 'heavy-1', 'light-1'])
    check(rc == 2 and spans(trace) == {}, f'and the run with it, nothing run ({rc})')
    linked = tmp / 'linked'
    linked.mkdir(mode=0o700)
    victim = tmp / 'victim.txt'
    victim.write_text('the user\'s own file\n')
    (linked / 'slot-1').symlink_to(victim)
    try:
        cr.Slots(1, linked).take('mine')
        check(False, 'a slot that is a link is refused')
    except cr.Refused as e:
        check(True, f'a slot that is a link is refused ({e})')
    check(victim.read_text() == 'the user\'s own file\n', 'the file the link points to is as it was')
    cr.SLOTS_DIR = linked
    trace.write_text('')
    rc = cr.main(['-j', '2', 'heavy-1', 'light-1'])
    sp = spans(trace)
    check(rc == 1 and 'heavy-1' not in sp and 'light-1' in sp,
          f'in a run, its check fails and is not run, the rest are ({rc}, {sorted(sp)})')
    check(victim.read_text() == 'the user\'s own file\n', 'and the file is still as it was')
    subprocess.call(['rm', '-rf', str(tmp)])


def main():
    cr = load()
    shards(cr)
    slots(cr)
    print(f'check-checkrun: {"all passed" if not fails else f"{len(fails)} FAILED"}')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
