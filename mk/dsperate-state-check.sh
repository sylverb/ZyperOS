#!/bin/sh
# A save state from the previous DSperate (v3.0.5) in a built DSperate core:
# tools/dsperate-state-check.c loads the core as Diatom does, boots a ROM it
# writes itself, whose ARM9 counts VBlanks in main RAM, and loads
# tools/dsperate-state-v305.state.gz, a state the v3.0.5 core made of that
# ROM. The core must take it, main RAM must be the state's, and the game must
# run on from the saved point. Diatom hands such a state to the newer core
# (diatom/src/save.c, STATE_READS) on the strength of this.
#
#   sh mk/dsperate-state-check.sh [core.so]    default vendor/cores/dsperate_libretro.so
#
# mk/build-dsperate.sh runs it on each core it builds, before installing it.
# Compiled and run as mk/dsperate-rtc-check.sh is: the build's own image
# (dsperate-lr-cross) compiles, ubuntu:24.04 for arm64 runs. A few seconds.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SO=${1:-$ROOT/vendor/cores/dsperate_libretro.so}
IMAGE=dsperate-lr-cross

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
[ -f "$SO" ] || { echo "no core at $SO" >&2; exit 1; }
docker image inspect $IMAGE >/dev/null 2>&1 ||
	{ echo "no $IMAGE image: mk/build-dsperate.sh builds it" >&2; exit 1; }

W=$(mktemp -d "${TMPDIR:-/tmp}/dsperate-state.XXXXXX")
trap 'rm -rf "$W"' EXIT
cp "$SO" "$W/core.so"
gzip -dc "$ROOT/tools/dsperate-state-v305.state.gz" > "$W/v305.state"
docker run --rm --platform linux/amd64 -v "$ROOT:/src:ro" -v "$W:/w" $IMAGE \
	aarch64-linux-gnu-gcc -std=gnu11 -O1 -Wall -Wextra -Werror -I/src/diatom/src \
	-o /w/check /src/tools/dsperate-state-check.c -ldl
docker run --rm --platform linux/arm64 -v "$W:/w" ubuntu:24.04 \
	/w/check /w/core.so /w /w/v305.state
