#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The boot screens at the player's brightness (src/boottheme.h): plastron's
# init (package/zyperos/zyperos.init, bright_keep) sets each backlight to the
# card's .userdata/<device>/boot-brightness as soon as the card is mounted,
# within the backlight's own maximum, or the Brick's through its display
# engine's tool; and leaves the light as it is for no file, "-", 0, a
# leading zero, a word, or a value above the maximum. Run on a scratch card
# and scratch backlights, the function taken from the init as it ships.
set -u

INIT=plastron/buildroot/package/zyperos/zyperos.init
D=build-native/bootbright
fails=0
fail() { echo "  FAIL $*"; fails=$((fails + 1)); }

rm -rf "$D"
mkdir -p "$D" || exit 1
sed -n '/^bright_keep() {$/,/^}$/p' "$INIT" > "$D/fn.sh"
grep -q '^bright_keep() {' "$D/fn.sh" || { echo "FAIL no bright_keep in $INIT"; exit 1; }

# A scratch card with `value` in its file (none for "none"), a backlight of
# maximum 255 at 200, as the Pixel 2's kernel lights it, and maybe a Brick tool
setup() {
	rm -rf "$D/card" "$D/lights" "$D/brick"
	mkdir -p "$D/card/.userdata/pixel2" "$D/lights/backlight"
	echo 255 > "$D/lights/backlight/max_brightness"
	echo 200 > "$D/lights/backlight/brightness"
	[ "$1" = none ] || printf '%s\n' "$1" > "$D/card/.userdata/pixel2/boot-brightness"
}

run() {
	(
		CARD=$D/card
		ZYPEROS_BACKLIGHTS=$D/lights
		ZYPEROS_BRICK_BACKLIGHT=$D/brick
		. "$D/fn.sh"
		bright_keep
	)
}

level() { read -r v < "$D/lights/backlight/brightness"; echo "$v"; }

setup 74; run
[ "$(level)" = 74 ] || fail "the card's 74 is the backlight's, got $(level)"
setup 255; run
[ "$(level)" = 255 ] || fail "the maximum itself is taken"
for bad in none - 0 017 abc 256 '' 7x; do
	setup "$bad"; run
	[ "$(level)" = 200 ] || fail "'$bad' leaves the kernel's level, got $(level)"
done

# The Brick: its display engine's tool, not the sysfs class
setup 96
printf '#!/bin/sh\necho "$1" > "%s/brick.got"\n' "$D" > "$D/brick"
chmod +x "$D/brick"
run
[ -f "$D/brick.got" ] && [ "$(cat "$D/brick.got")" = 96 ] || fail "the Brick's tool is given 96"
[ "$(level)" = 200 ] || fail "and the sysfs class is left alone there"

if [ "$fails" -gt 0 ]; then
	echo "bootbright: $fails failed"
	exit 1
fi
echo "ok: the card's brightness reaches the backlight at boot, and nothing else does"
