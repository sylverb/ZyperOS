#!/bin/sh
# The PlayStation's memory card across a resume, in a PCSX ReARMed core:
# tools/pcsx-memcard-check.c loads the core as Diatom does, boots a program it
# writes itself, which writes the card in slot 1, and loads the core's state of
# it over the same card (FLAG must be the saved one, not "new card") and over a
# changed one (FLAG must be "new card"). Then states cross between the core and
# the buildbot's build of the same commit, both ways, and must load.
# mk/patches/pcsx_rearmed-memcard-resume.patch is what makes the first hold;
# the buildbot core fails it.
#
#   sh mk/pcsx-memcard-check.sh [core.so [other.so]]
#
# core.so is vendor/cores/pcsx_rearmed_libretro.so unless given. other.so is
# the buildbot core ZyperOS shipped up to 1.0.5, from the cores release it is
# still in, held to its sha256.
#
# mk/build-pcsx-rearmed.sh runs it on the core it builds, before installing
# it, with its own build of the unpatched tree as other.so. Compiled and run
# in the image that builds the core (mk/pcsx-rearmed.Dockerfile, linux/arm64).
# A few seconds.
set -e

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SO=${1:-$ROOT/vendor/cores/pcsx_rearmed_libretro.so}
OTHER=${2:-}
IMAGE=zyperos-pcsx-rearmed
BUILDBOT=https://github.com/tarikbc/ZyperOS/releases/download/cores-2026-11-16/pcsx_rearmed_libretro.so
BUILDBOT_SHA=07cb263f1af645ea60906dbdc8d00ba32cd735bde60cf7d8fbc210118e169808

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
[ -f "$SO" ] || { echo "no core at $SO" >&2; exit 1; }
docker image inspect $IMAGE >/dev/null 2>&1 ||
	docker build --platform linux/arm64 -q -f "$ROOT/mk/pcsx-rearmed.Dockerfile" -t $IMAGE "$ROOT/mk" >/dev/null

W=$(mktemp -d "${TMPDIR:-/tmp}/pcsx-memcard.XXXXXX")
trap 'rm -rf "$W"' EXIT
cp "$SO" "$W/core.so"
if [ -n "$OTHER" ]; then
	cp "$OTHER" "$W/other.so"
else
	# Into the check's own folder, and the copy it runs is the one checked
	curl -sSfL -o "$W/other.so" "$BUILDBOT"
	[ "$(shasum -a 256 "$W/other.so" | cut -d' ' -f1)" = $BUILDBOT_SHA ] ||
		{ echo "the buildbot core is not $BUILDBOT_SHA" >&2; exit 1; }
fi
docker run --rm --platform linux/arm64 -v "$ROOT:/src:ro" -v "$W:/w" $IMAGE sh -c '
gcc-12 -std=gnu11 -O1 -Wall -Wextra -Werror -I/src/diatom/src \
	-o /w/check /src/tools/pcsx-memcard-check.c -ldl &&
/w/check /w/core.so /w /w/other.so'
