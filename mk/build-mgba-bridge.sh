#!/bin/sh
# Build the mGBA core ZyperOS ships UNTIL LIBRETRO SYNCS ITS FORK.
#
# THIS SCRIPT IS MEANT TO BE DELETED. Every core ZyperOS ships is an official
# libretro buildbot binary, fetched and hash-pinned by mk/fetch-vendor.sh. This
# is the one exception, and it exists for a reason with an expiry date:
#
#   mGBA's buildbot core segfaults on the first read of cartridge RAM for every
#   MBC2 Game Boy cartridge - Kirby's Pinball Land, Wave Race, Golf, X, both
#   Final Fantasy Legends. A regression in mgba a1b2b23 (2026-07-30), reported
#   as mgba-emu/mgba#3859 and fixed upstream the SAME DAY in 543a1975.
#
#   The fix is in no core that can be downloaded. libretro builds from its own
#   fork, github.com/libretro/mgba, unsynced since 2026-08-06.
#
# So this builds libretro's own tree at the exact commit the pin is built from,
# applies only upstream's own fixes, and hardens it to match the official
# build. No patch of ours is in it. Since the bridge exists anyway, it carries
# two more of upstream's fixes for bugs the pinned tree has, each as upstream
# wrote it:
#
#   685023e  GBA audio: a FIFO DMA the game programs 16-bit stepped its source
#            by 2 bytes while moving 4-byte words (since upstream 2deb4aa6f
#            cached the offsets), so the FIFO played doubled, misaligned
#            samples: a ~100 Hz buzz under the music of the Classic NES Series
#            and the e-Reader NES cards. Games that program it 32-bit, most
#            of them, never meet it.
#   8485192  Core: a custom RTC source read its own data from the wrong offset
#            in a save state (sizeof(state), a pointer, for sizeof(*state)).
#            Only the load side changes; what a state holds does not.
#
# WHEN LIBRETRO SYNCS: restore the `fetch_core mgba` line in fetch-vendor.sh,
# delete this script, and verify a save state made on this core still loads on
# the official one, and that the synced tree has all three fixes.
#
# Needs: docker. Its image is mk/mgba-bridge.Dockerfile, built here; after
# the first time, Docker has it cached.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=${1:-/tmp/zyperos-mgba-bridge}
PIN=e31759b24e7a4e3899285ff720d7b573ac328ae7   # libretro/mgba, what CORES.md pins
# mgba-emu/mgba's fixes, each its commit and the one source file it is applied
# to (543a197 also changes the Qt frontend, which the core does not build)
FIXES="543a197582c30364584d773a974d7f991892fa43:src/gb/mbc.c
685023e05d90d87050fb357f46f7bd2d907083f5:src/gba/audio.c
8485192285a42e8c4d68c0d372ef9c1b6008d80a:src/core/interface.c"
SHA=fa58e9e986344d446e4cf21ef874ec88f8a93aefd287ae0c624a5a6dec839b06   # the core this builds

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
docker build -q -f "$ROOT/mk/mgba-bridge.Dockerfile" -t mgba-xbuild "$ROOT/mk" >/dev/null

if [ ! -d "$SRC/.git" ]; then
	git clone --depth 40 -b master https://github.com/libretro/mgba.git "$SRC"
fi
cd "$SRC"
git fetch --depth 40 origin master
git checkout -q "$PIN"
git remote get-url up >/dev/null 2>&1 || git remote add up https://github.com/mgba-emu/mgba.git
# Each fix by its hash, not master's tip: they left the last few commits of
# master long ago. Two deep so git show has its parent to diff against. Every
# file back to the pin first, so a second run applies each fix once.
for fix in $FIXES; do git checkout -q -- "${fix#*:}"; done
for fix in $FIXES; do
	git fetch -q --depth 2 up "${fix%%:*}"
	git show "${fix%%:*}" -- "${fix#*:}" | git apply
done
# project(mGBA) declares C and CXX; there is no cross g++ in the image and the
# libretro core is pure C, so it does not need one. perl rather than sed -i,
# which takes a backup suffix on BSD and not on GNU - written with sed first,
# where it silently did nothing on macOS and CMake then asked for a C++
# compiler. No `|| true` either: if this edit fails the build must stop.
perl -pi -e 's/^project\(mGBA\)$/project(mGBA C)/' CMakeLists.txt
grep -q '^project(mGBA C)$' CMakeLists.txt || { echo "!! CMakeLists edit failed" >&2; exit 1; }

# -DZLIB_LIBRARIES is not a typo for the singular. mGBA's CMake has a gap: the
# libretro target appends $ZLIB_LIBRARIES to OS_LIB, but the bundled-zlib path
# sets ZLIB_LIBRARY, so zlib is compiled and never linked. The core then builds,
# loads, and dies at dlopen with "undefined symbol: crc32".
docker run --rm -v "$SRC:/src" mgba-xbuild bash -lc '
set -e
HARD="-D_FORTIFY_SOURCE=2 -fstack-protector-strong -O2"
cmake -S /src -B /src/bl -DBUILD_LIBRETRO=ON -DSKIP_LIBRARY=ON \
  -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
  -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="$HARD" \
  -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON -DZLIB_LIBRARIES=zlibstatic \
  -DUSE_FFMPEG=OFF -DUSE_PNG=OFF -DUSE_LIBZIP=OFF -DUSE_SQLITE3=OFF \
  -DUSE_ELF=OFF -DUSE_EPOXY=OFF -DUSE_DISCORD_RPC=OFF -DUSE_LZMA=OFF \
  -DBUILD_QT=OFF -DBUILD_SDL=OFF >/dev/null
cmake --build /src/bl -j$(nproc) >/dev/null
aarch64-linux-gnu-strip --strip-unneeded /src/bl/mgba_libretro.so
# Never ship one that cannot load. Two cores were deployed and called "tested"
# before anyone noticed they had never linked at all.
[ "$(aarch64-linux-gnu-nm -D -u /src/bl/mgba_libretro.so | grep -c crc32)" = 0 ] \
  || { echo "!! undefined crc32 - zlib did not link" >&2; exit 1; }
[ "$(aarch64-linux-gnu-nm -D -u /src/bl/mgba_libretro.so | grep -cE "__stack_chk|_chk@")" -ge 9 ] \
  || { echo "!! not hardened - the official build has 9 such symbols" >&2; exit 1; }
'
# The image is pinned by digest and its packages by snapshot, and the build
# has no git, so the same tree gives the same bytes: two clean clones, two
# builds, one sha256 (2026-11-16). The 1.0.x cards' mgba was this script's
# build with one fix, the same build ID, without the .comment section their
# Buildroot strip takes off.
GOT=$(shasum -a 256 "$SRC/bl/mgba_libretro.so" | cut -d' ' -f1)
if [ "$GOT" != "$SHA" ]; then
	echo "!! mgba sha256 $GOT, expected $SHA" >&2
	echo "  The toolchain image, the pin or a fix changed. Verify the core, then" >&2
	echo "  update SHA here." >&2
	exit 1
fi
mkdir -p "$ROOT/vendor/cores"
cp "$SRC/bl/mgba_libretro.so" "$ROOT/vendor/cores/mgba_libretro.so"
echo "  built  mgba (bridge, $PIN +$(for fix in $FIXES; do printf ' %.7s' "$fix"; done))"
echo "  sha256 $GOT"
