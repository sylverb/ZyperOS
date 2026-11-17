#!/bin/sh
# Build the PlayStation core, PCSX ReARMed, with ZyperOS's patches.
#
# Up to 1.0.5 ZyperOS shipped libretro's buildbot build of
# libretro/pcsx_rearmed 7b0e8f8 (07cb263f..., still in the cores-2026-11-16
# release). That core marks both memory cards "new" after every state load
# (libpcsxcore/sio.c, sioFreeze, since upstream 886a15b), and ZyperOS starts
# every game from its auto state, so at each start a game saw another card
# put in: Rayman said THE MEMORY CARD HAS CHANGED and lost its save (reported
# on Discord, 1.0.5). mk/patches/pcsx_rearmed-memcard-resume.patch keeps each
# card's hash and FLAG in the state: a card that is the same when the state
# is loaded gets its FLAG back, and a changed one is still marked new.
#
# The patches are the only difference from the core ZyperOS shipped. The
# buildbot's compiler (mk/pcsx-rearmed.Dockerfile: Ubuntu 22.04's gcc-12
# 12.3.0, as the core's .comment says), its Makefile.libretro platform=unix
# (ari64, gpu=neon), its git describe (r26l-137-g7b0e8f8, which the state
# header and the start line carry; a tarball has no git, so a stand-in answers
# for it) and its build date (frontend/main.c prints __DATE__, so
# SOURCE_DATE_EPOCH is the date that core prints): with those, the tree
# WITHOUT the patches builds the buildbot's file byte for byte, and this
# script stops if it does not. Then the patched tree, which
# mk/pcsx-memcard-check.sh checks against the unpatched one before it is
# installed, held to its sha256 like every core ZyperOS builds.
#
# Usage: mk/build-pcsx-rearmed.sh [work dir]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=${1:-/tmp/zyperos-pcsx-rearmed}
PIN=7b0e8f8fadfbce3d170d195b9d9eecaabfa08c8e
TAR_SHA=4f47bd98bac6cdf62dd4f7a56069b0b0d9999aa6800e636334d1788c5cf38ccf
BUILDBOT_SHA=07cb263f1af645ea60906dbdc8d00ba32cd735bde60cf7d8fbc210118e169808   # PIN as the buildbot built it
SHA=1e688ef6bcc8397e71d6de71b0d63d0ad7aa24249a2c5f56bd664ea99e419c26            # PIN with mk/patches/pcsx_rearmed-*
DESCRIBE=r26l-137-g7b0e8f8
EPOCH=1793689647            # "build time: Nov  3 2026 07:07:27", the buildbot core's
IMAGE=zyperos-pcsx-rearmed
SO=pcsx_rearmed_libretro.so

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
command -v curl   >/dev/null || { echo "need curl" >&2; exit 1; }
docker build --platform linux/arm64 -q -f "$ROOT/mk/pcsx-rearmed.Dockerfile" -t $IMAGE "$ROOT/mk" >/dev/null

mkdir -p "$WORK"
TAR="$WORK/pcsx_rearmed-$PIN.tar.gz"
[ -f "$TAR" ] || curl -sSfL -o "$TAR" "https://github.com/libretro/pcsx_rearmed/archive/$PIN.tar.gz"
GOT=$(shasum -a 256 "$TAR" | cut -d' ' -f1)
[ "$GOT" = "$TAR_SHA" ] || { echo "!! source sha256 $GOT, expected $TAR_SHA" >&2; exit 1; }

# The tree in $1, built and stripped as the buildbot's core is
build() {
	rm -rf "$1"
	mkdir -p "$1"
	tar -xzf "$TAR" -C "$1" --strip-components=1
	if [ -n "$2" ]; then
		for p in "$ROOT"/mk/patches/pcsx_rearmed-*.patch; do patch -s -p1 -d "$1" < "$p"; done
	fi
	docker run --rm --platform linux/arm64 -v "$1:/builds/libretro/pcsx_rearmed" \
		-e DESCRIBE=$DESCRIBE -e EPOCH=$EPOCH $IMAGE bash -c '
set -e
printf "#!/bin/sh\ncase \"\$*\" in\n\"describe --always\") echo %s ;;\n\"rev-parse --short HEAD\") echo %s ;;\n*) exit 1 ;;\nesac\n" \
	"$DESCRIBE" "${DESCRIBE##*-g}" > /usr/local/bin/git
chmod +x /usr/local/bin/git
make -j3 -f Makefile.libretro platform=unix CC=gcc-12 SOURCE_DATE_EPOCH=$EPOCH > build.log 2>&1 ||
	{ tail -30 build.log >&2; exit 1; }
strip pcsx_rearmed_libretro.so
[ "$(nm -D -u pcsx_rearmed_libretro.so | grep -v " w " | grep -vc "@")" = 0 ] \
  || { echo "!! undefined symbols in pcsx_rearmed_libretro.so - it will not dlopen" >&2; exit 1; }
'
}

build "$WORK/plain" ""
GOT=$(shasum -a 256 "$WORK/plain/$SO" | cut -d' ' -f1)
if [ "$GOT" != "$BUILDBOT_SHA" ]; then
	echo "!! without the patches the tree built $GOT, not the buildbot's $BUILDBOT_SHA:" >&2
	echo "  the image is not the toolchain the buildbot used, so the patched core would" >&2
	echo "  differ in more than its patches" >&2
	exit 1
fi
echo "  same   without the patches, the buildbot's core byte for byte ($BUILDBOT_SHA)"

build "$WORK/patched" patched
sh "$ROOT/mk/pcsx-memcard-check.sh" "$WORK/patched/$SO" "$WORK/plain/$SO"
GOT=$(shasum -a 256 "$WORK/patched/$SO" | cut -d' ' -f1)
if [ "$GOT" != "$SHA" ]; then
	echo "!! pcsx_rearmed sha256 $GOT, expected $SHA" >&2
	echo "  The patches changed. Verify the core, then update its sha256 here." >&2
	exit 1
fi
mkdir -p "$ROOT/vendor/cores"
# The copy is what is held to the pin, so nothing in the work dir can change
# between the check and the install
cp "$WORK/patched/$SO" "$ROOT/vendor/cores/$SO.new"
GOT=$(shasum -a 256 "$ROOT/vendor/cores/$SO.new" | cut -d' ' -f1)
[ "$GOT" = "$SHA" ] ||
	{ rm -f "$ROOT/vendor/cores/$SO.new"; echo "!! the copy is $GOT, not $SHA" >&2; exit 1; }
mv "$ROOT/vendor/cores/$SO.new" "$ROOT/vendor/cores/$SO"
echo "  built  pcsx_rearmed ($PIN, with its patches)"
# Which patches are in it, for mk/payload.sh to hold it to
sh "$ROOT/mk/core-stamp.sh" write pcsx_rearmed
