#!/usr/bin/env python3
"""check-first-frame.py VIDEO [VIDEO ...] [--min-stddev 12] [--min-range 60]

Frame 0 is the picture Discord, Reddit, most players and a page without a
poster show until someone presses play. A video that fades in from the ground
colour, from black or white, or that waits for its parts to arrive, shows a
blank card there (the 1.0.6 Discord video did: one flat beige).

For each video this takes frame 0 (ffmpeg -frames:v 1), saves it beside the
video as VIDEO-poster.jpg (the poster for a page's <video poster>, or to look
at), and measures it at 160x90 in grey: the standard deviation of the pixels
and their range. A designed frame (a title, a handheld, a screen) measures a
stddev of 30 or more; a flat or nearly flat one is under 5. It exits 1, and
says which, if any video's frame 0 is under either threshold.

Uses only ffmpeg and plain Python.
"""
import argparse
import math
import subprocess
import sys
from pathlib import Path


def frame0_stats(video, poster):
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', str(video), '-frames:v', '1', '-q:v', '3', str(poster)],
                   check=True)
    raw = subprocess.run(['ffmpeg', '-loglevel', 'error', '-i', str(poster), '-vf', 'scale=160:90,format=gray',
                          '-f', 'rawvideo', '-'], check=True, capture_output=True).stdout
    n = len(raw)
    mean = sum(raw) / n
    sd = math.sqrt(sum((x - mean) ** 2 for x in raw) / n)
    return mean, sd, min(raw), max(raw)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('videos', nargs='+', type=Path)
    ap.add_argument('--min-stddev', type=float, default=12)
    ap.add_argument('--min-range', type=int, default=60)
    args = ap.parse_args()
    blank = []
    for v in args.videos:
        poster = v.with_name(v.stem + '-poster.jpg')
        mean, sd, lo, hi = frame0_stats(v, poster)
        ok = sd >= args.min_stddev and hi - lo >= args.min_range
        print(f'{"ok   " if ok else "BLANK"} {v}: frame 0 mean {mean:.0f}, stddev {sd:.1f}, range {lo}-{hi}; poster {poster}')
        if not ok:
            blank.append(v)
    if blank:
        print(f'{len(blank)} video(s) open on a blank frame: make frame 0 a designed frame (video/README.md, '
              '"Frame 0") and render again', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
