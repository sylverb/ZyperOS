#!/bin/sh
# Engine B's extended backgrounds in a built DSperate core, frame for frame
# against the core without mk/patches/dsperate-lr-ext-bg.patch:
# tools/dsperate-extbg-check.c loads the core as Diatom does, boots a ROM it
# writes itself, shows rotated and scaled bitmaps, direct colour and tile maps
# in four VRAM bank layouts, and holds each of the 96 frames' hashes to the
# ones the release core made before the patch.
#
#   sh mk/dsperate-extbg-check.sh [core.so]    default vendor/cores/dsperate_libretro.so
#
# mk/build-dsperate.sh runs it on each core it builds, before installing it.
# Compiled with the build's own image (dsperate-lr-cross) and run in
# ubuntu:24.04 for arm64, as mk/dsperate-rtc-check.sh is. A few seconds.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SO=${1:-$ROOT/vendor/cores/dsperate_libretro.so}
IMAGE=dsperate-lr-cross

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
[ -f "$SO" ] || { echo "no core at $SO" >&2; exit 1; }
docker image inspect $IMAGE >/dev/null 2>&1 ||
	{ echo "no $IMAGE image: mk/build-dsperate.sh builds it" >&2; exit 1; }
W=$(mktemp -d "${TMPDIR:-/tmp}/dsperate-extbg.XXXXXX")
trap 'rm -rf "$W"' EXIT
cp "$SO" "$W/core.so"
docker run --rm --platform linux/amd64 -v "$ROOT:/src:ro" -v "$W:/w" $IMAGE \
	aarch64-linux-gnu-gcc -std=gnu11 -O1 -Wall -Wextra -Werror -I/src/diatom/src \
	-o /w/check /src/tools/dsperate-extbg-check.c -ldl
rc=0
docker run --rm --platform linux/arm64 -v "$W:/w" ubuntu:24.04 /w/check /w/core.so /w > "$W/out" 2>&1 || rc=$?
grep -v '^    core: ' "$W/out" || true
exit $rc
