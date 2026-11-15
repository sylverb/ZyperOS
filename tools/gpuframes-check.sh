#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The systems whose cores need GPU frames (config/systems.cfg's needs lines)
# on a handheld without them: on the Brick's profile a card with an NES, a
# Dreamcast and an N64 game shows the NES shelf alone, and says the two it
# left out; on the Pixel 2's, all three, as before. The card is a scratch
# tree under build-native/gpuframes-card; the launcher scans it for a shot.
#
# Needs the launcher built, which needs SDL2, and skips cleanly without it.
set -u

D=build-native/gpuframes-card
BIN=build-native/zyperos

if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf 2>/dev/null; then
	echo "skipped: no SDL2 here to build the launcher with"
	exit 0
fi
if [ ! -x "$BIN" ]; then
	echo "FAIL no $BIN: make native first" >&2
	exit 1
fi

rm -rf "$D"
mkdir -p "$D/root" "$D/Roms/NES" "$D/Roms/Dreamcast" "$D/Roms/Nintendo 64" "$D/userdata" "$D/shared" || exit 1
cp config/systems.cfg "$D/root/systems.cfg" || exit 1
: >"$D/Roms/NES/Game.nes" || exit 1
: >"$D/Roms/Dreamcast/Game.chd" || exit 1
: >"$D/Roms/Nintendo 64/Game.z64" || exit 1

# The scan's lines for a shot of the home on device $1
scan() {
	ZYPEROS_HEADLESS=1 SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
	ZYPEROS_ROOT="$D/root" ZYPEROS_CARD="$D" ZYPEROS_ROMS="$D/Roms" \
	ZYPEROS_USERDATA="$D/userdata" ZYPEROS_SHARED="$D/shared" ZYPEROS_WEB="$D" \
	ZYPEROS_FONT=res/fonts/menu.ttf "$BIN" --device "$1" --shot "$D/home-$1.png" 2>&1 | grep '^scan: '
}

fails=0
fail() { echo "  FAIL $*"; fails=$((fails + 1)); }

out=$(scan brick)
echo "$out" | grep -q '^scan: 3 of [0-9]* systems have games$' || fail "the Brick: three systems have games"
echo "$out" | grep -q '^scan: 2 of them need GPU frames, which this handheld has not$' ||
	fail "the Brick: the Dreamcast and the N64 left out for GPU frames"
out=$(scan pixel2)
echo "$out" | grep -q '^scan: 3 of [0-9]* systems have games$' || fail "the Pixel 2: three systems have games"
echo "$out" | grep -q 'GPU frames' && fail "the Pixel 2: no shelf left out for GPU frames"
[ -s "$D/home-brick.png" ] && [ -s "$D/home-pixel2.png" ] || fail "both homes shot"

if [ "$fails" -gt 0 ]; then
	echo "gpuframes: $fails failed"
	exit 1
fi
echo "ok: no GPU-frame shelf where there are no GPU frames, every shelf where there are"
