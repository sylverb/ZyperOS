#!/bin/sh
# The Nintendo DS clock in a built DSperate core: tools/dsperate-rtc-check.c
# loads the core as Diatom does, boots a ROM it writes itself, and reads the
# RTC out of the core's states - the host's local time at the start, after
# frames run faster than real time, after a pause, after a state saved
# earlier is loaded, after a reset and in a second game. The core that
# shipped before mk/patches/dsperate-lr-rtc.patch fails each one: its clock
# is stopped at 2000-01-01 00:00:00.
#
#   sh mk/dsperate-rtc-check.sh [core.so]    default vendor/cores/dsperate_libretro.so
#
# mk/build-dsperate.sh runs it on each core it builds, before installing it.
# The check is compiled with the build's own image (dsperate-lr-cross, its
# aarch64 compiler on Ubuntu 24.04) and runs in ubuntu:24.04 for arm64, which
# has the core's glibc and libstdc++ (an Arm Mac runs it natively). It runs
# in UTC-3, so a core reading UTC instead of the local time fails. About
# fifteen seconds, most of them the waits it measures.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SO=${1:-$ROOT/vendor/cores/dsperate_libretro.so}
IMAGE=dsperate-lr-cross

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
[ -f "$SO" ] || { echo "no core at $SO" >&2; exit 1; }
docker image inspect $IMAGE >/dev/null 2>&1 ||
	{ echo "no $IMAGE image: mk/build-dsperate.sh builds it" >&2; exit 1; }
W=$(mktemp -d "${TMPDIR:-/tmp}/dsperate-rtc.XXXXXX")
trap 'rm -rf "$W"' EXIT
cp "$SO" "$W/core.so"
docker run --rm --platform linux/amd64 -v "$ROOT:/src:ro" -v "$W:/w" $IMAGE \
	aarch64-linux-gnu-gcc -std=gnu11 -O1 -Wall -Wextra -Werror -I/src/diatom/src \
	-o /w/check /src/tools/dsperate-rtc-check.c -ldl
docker run --rm --platform linux/arm64 -e TZ='<-03>3' -v "$W:/w" ubuntu:24.04 \
	/w/check /w/core.so /w
