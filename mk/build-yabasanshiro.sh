#!/bin/sh
# Build the Sega Saturn core, yabasanshiro_libretro.so: YabaSanshiro, the
# Saturn emulator with an ARM64 dynarec for its two SH-2s and an OpenGL ES 3
# renderer, which is what makes the Saturn run on an RK3326 at all. Beetle
# Saturn, the other libretro Saturn core, interprets everything and wants a
# desktop CPU several times this one's speed. Both need the Saturn's BIOS:
# this YabaSanshiro has a high-level one, but refuses to start a disc
# without the real file (config/systems.cfg).
#
# libretro/yabause at 39535a6 (its yabasanshiro branch, 2024-05-23, gcc 14
# fixes), ROCKNIX's yabasanshiro-lr pin, GPL-2.0-or-later. libretro's buildbot
# builds it for aarch64 with desktop OpenGL, which the devices do not have, so
# it is built here for OpenGL ES 3, drawn in Diatom's GPU frames as the N64
# core is: platform=arm64_cortex_a53_gles3, the core's own target for a small
# ARM64 device (a Cortex-A35 runs what an A53 does), with the aarch64 dynarec
# (USE_AARCH64_DRC, DYNAREC_DEVMIYAX). ROCKNIX's speed flags too: -Ofast
# -ffast-math where the Makefile says -O3, link-time optimization, and no
# outline atomics (gcc 10's default on aarch64, a call per atomic).
#
# In mk/flycast2021.Dockerfile: bullseye's gcc 10 on arm64, libGLESv2 to link
# against. Checked as build-mupen64plus-next.sh checks its core: no unversioned
# undefined symbol but OpenGL ES's and EGL's own. Then against the sha256 here.
#
# Usage: mk/build-yabasanshiro.sh [work dir]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=${1:-/tmp/zyperos-yabasanshiro}
PIN=39535a6abcad5abf9f71c8b2a7975f005ee12ed6   # libretro/yabause, ROCKNIX's yabasanshiro-lr pin
TAR_SHA=56aa4dc17f1c9e61e60274a66c71b33371786688df001ac0ca56569d94cc8b3b
SHA=7b53e41fc2e257b9933efcae017764b7be8050415c6ca43fa210026e93c249a3

command -v docker >/dev/null || { echo "need docker" >&2; exit 1; }
command -v curl   >/dev/null || { echo "need curl" >&2; exit 1; }
docker build --platform linux/arm64 -q -f "$ROOT/mk/flycast2021.Dockerfile" -t zyperos-flycast2021 "$ROOT/mk" >/dev/null

mkdir -p "$SRC"
TAR="$SRC/yabause-$PIN.tar.gz"
[ -f "$TAR" ] || curl -sSfL -o "$TAR" "https://github.com/libretro/yabause/archive/$PIN.tar.gz"
GOT=$(shasum -a 256 "$TAR" | cut -d' ' -f1)
[ "$GOT" = "$TAR_SHA" ] || { echo "!! source sha256 $GOT, expected $TAR_SHA" >&2; exit 1; }
rm -rf "$SRC/yabause-$PIN"
tar -xzf "$TAR" -C "$SRC"
# And one of ours, mk/patches/yabasanshiro-unload.patch (1.0.6). The core's
# retro_unload_game takes its renderer down (YabauseDeInit, VIDCore NULL)
# but leaves renderer_running set, so the context_destroy a frontend calls
# after it ran VIDCore->DeInit through NULL: every Saturn quit crashed the
# resident Diatom (SIGSEGV at 0x18, the Pixel 2, 2026-11-15), after the auto
# state was written, and the next game ran without its in-game menu. And the
# core set its emulator up only on the first context_reset of the library
# (first_ctx_reset), so a second Saturn game in one resident Diatom would
# have started on an emulator the last unload took down. The unload now says
# both: the renderer is down, and the next context_reset starts afresh.
# patch, not git apply: git apply in a work dir inside a repository skips
# what is outside its folder and says nothing
patch -d "$SRC/yabause-$PIN" -p1 --forward --quiet < "$ROOT/mk/patches/yabasanshiro-unload.patch"
grep -q "first_ctx_reset = 1;" "$SRC/yabause-$PIN/yabause/src/libretro/libretro.c" \
  || { echo "!! yabasanshiro-unload.patch did not apply" >&2; exit 1; }
# And two for its speed on the Pixel 2 (1.1.0), each timed in the core on
# Virtua Fighter 2 and Panzer Dragoon II (the patches' preambles have it):
#   mk/patches/yabasanshiro-fastforward.patch  the auto frame skip on again
#                                              when fast forward ends (it
#                                              stayed off: 0.30x -> 0.20x)
#   mk/patches/yabasanshiro-atlas.patch        the texture atlas in the CPU's
#                                              own memory, uploaded from there
#                                              (Panfrost read the pixel buffer
#                                              back from uncached memory)
patch -d "$SRC/yabause-$PIN" -p1 --forward --quiet < "$ROOT/mk/patches/yabasanshiro-fastforward.patch"
grep -q "ff_was = fastforward;" "$SRC/yabause-$PIN/yabause/src/libretro/libretro.c" \
  || { echo "!! yabasanshiro-fastforward.patch did not apply" >&2; exit 1; }
patch -d "$SRC/yabause-$PIN" -p1 --forward --quiet < "$ROOT/mk/patches/yabasanshiro-atlas.patch"
grep -q "Fail to grow the atlas" "$SRC/yabause-$PIN/yabause/src/ygles.c" \
  || { echo "!! yabasanshiro-atlas.patch did not apply" >&2; exit 1; }

# The source goes in read-only and is built in a copy inside the container,
# and only the core comes out, to $SRC/out. Built where it was mounted, the
# sed -i below failed on Docker Desktop's shared folders: sed makes its
# temporary file beside the Makefile, and there that open is refused
# ("couldn't open temporary file ./sedXXXXXX: Permission denied", 2026-11-15)
# although plain writes work. The copy builds the same core, byte for byte.
rm -rf "$SRC/out"
mkdir -p "$SRC/out"
docker run --rm --platform linux/arm64 -v "$SRC/yabause-$PIN:/src:ro" -v "$SRC/out:/out" \
	zyperos-flycast2021 bash -c '
set -e
cp -a /src /build
cd /build/yabause/src/libretro
sed -i "s/-O[23]/-Ofast -ffast-math/" Makefile
export CFLAGS="-flto -fipa-pta -mno-outline-atomics"
export CXXFLAGS="$CFLAGS" LDFLAGS="-flto -fipa-pta"
make -j6 platform=arm64_cortex_a53_gles3 > build.log 2>&1 || { tail -30 build.log >&2; exit 1; }
so=yabasanshiro_libretro.so
strip --strip-unneeded $so
[ "$(nm -D -u $so | grep -v " w " | grep -v "@" | grep -vc " e\{0,1\}gl[A-Z]")" = 0 ] \
  || { nm -D -u $so | grep -v " w " | grep -v "@" | grep -v " e\{0,1\}gl[A-Z]" >&2;
       echo "!! undefined symbols - the core will not dlopen" >&2; exit 1; }
! readelf -d $so | grep -q "libGL\." \
  || { echo "!! $so links desktop OpenGL" >&2; exit 1; }
cp $so /out/
'
SO="$SRC/out/yabasanshiro_libretro.so"
GOT=$(shasum -a 256 "$SO" | cut -d' ' -f1)
if [ "$GOT" != "$SHA" ]; then
	echo "!! yabasanshiro sha256 $GOT, expected $SHA" >&2
	echo "  The toolchain image or the source changed. Verify the core, then" >&2
	echo "  update SHA here." >&2
	exit 1
fi
mkdir -p "$ROOT/vendor/cores"
cp "$SO" "$ROOT/vendor/cores/yabasanshiro_libretro.so"
echo "  built  yabasanshiro ($PIN)"
echo "  sha256 $GOT"
# Which patch is in it, for mk/payload.sh to hold it to (mk/core-stamp.sh)
sh "$ROOT/mk/core-stamp.sh" write yabasanshiro
