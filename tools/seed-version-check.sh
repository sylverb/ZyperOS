#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# make check-seed-version: the boot's ZyperOS version compare (plastron's
# package/zyperos/zyperos.init, version_of and newer_seed), the functions
# taken from the init as it ships and version_of the way each board's
# post-build.sh takes it to write SEED_V. A version with a suffix is a
# version: 1.1.0-rc.1, 1.0.7-kit.1 and 1.1.0-test.1 each give a number, below
# the release of the same three numbers as in System Update's su_version_cmp
# (src/sysupdate.c), kits below RCs as there, each kind by its number. A test
# image is below the kits and RCs of its numbers (su_version_cmp's letters put
# it above them), so the first of those pushed over it puts its ZyperOS
# folder on the card. What is not a version gives false, and
# never stops the shell. Run in whatever sh runs it: dash in CI's Linux job
# and in plastron's builder (post-build.sh), bash's sh on a Mac; the device's
# is BusyBox ash.
set -u

INIT=plastron/buildroot/package/zyperos/zyperos.init
D=build-native/seed-version
fails=0
fail() { echo "  FAIL $*"; fails=$((fails + 1)); }

rm -rf "$D"
mkdir -p "$D/card/ZyperOS" || exit 1
# The same range post-build.sh evals
VOF='/^version_of()/,/^}/p'
sed -n "$VOF" "$INIT" > "$D/version_of.sh"
sed -n '/^newer_seed() {$/,/^}$/p' "$INIT" > "$D/newer_seed.sh"
grep -q '^version_of() {' "$D/version_of.sh" || { echo "FAIL no version_of in $INIT"; exit 1; }
grep -q '^newer_seed() {' "$D/newer_seed.sh" || { echo "FAIL no newer_seed in $INIT"; exit 1; }
for b in px2 brick; do
	grep -qF "eval \"\$(sed -n '$VOF' \"\$INIT\")\"" "plastron/buildroot/board/$b/post-build.sh" ||
		fail "$b's post-build.sh takes version_of some other way than this check does"
done

# num STRING: the number version_of gives a VERSION file holding STRING, or
# "no" when it is not a version, or "died" when the shell stopped in it (an
# arithmetic error ends a script in ash). STRING "none" is no file at all.
num() {
	rm -f "$D/VERSION"
	[ "$1" = none ] || printf '%s\n' "$1" > "$D/VERSION"
	(
		. "$D/version_of.sh"
		if version_of "$D/VERSION"; then echo "${v:-empty}"; else echo no; fi
	) 2>/dev/null || echo died
}

# In order, each above the one before it
prev= prevn=
for s in 0.9.9 1.0.5 1.0.6 1.0.7-beta.1 1.0.7-test.1 1.0.7-test.2 1.0.7-kit.1 1.0.7-kit.2 \
	1.0.7-rc 1.0.7-rc.1 1.0.7 1.0.8 1.1.0-test.1 1.1.0-kit.1 1.1.0-kit.9 1.1.0-rc.0 \
	1.1.0-rc.1 1.1.0-rc.2 1.1.0-rc.10 1.1.0 1.1.1 1.2 1.10.0 2.0.0 10.0.0 999.999.999; do
	n=$(num "$s")
	case $n in no | died | empty) fail "'$s' is a version, got $n"; prev= ; continue ;; esac
	if [ -n "$prev" ] && ! [ "$n" -gt "$prevn" ]; then
		fail "'$s' ($n) is above '$prev' ($prevn)"
	fi
	prev=$s prevn=$n
done

# The same version written another way, and the suffix's number capped
same() {
	[ "$(num "$1")" = "$(num "$2")" ] || fail "'$1' ($(num "$1")) is '$2' ($(num "$2"))"
}
same 1.2 1.2.0
same 1.1.0-rc.08 1.1.0-rc.8
same 1.1.0-rc.5000 1.1.0-rc.999
same 1.1.0-test.x 1.1.0-test.0
[ "$(num 1.1.0-rc.999)" -lt "$(num 1.1.0)" ] || fail "rc.999 is below the release"

# The number itself, past 2^31: the shell's arithmetic and test are 64-bit
# (BusyBox's FEATURE_SH_MATH_64 and FEATURE_TEST_64 on both devices)
[ "$(num 1.1.0)" = 10010009999 ] || fail "1.1.0 is 10010009999, got $(num 1.1.0)"
[ "$(num 1.1.0-rc.2)" = 10010003002 ] || fail "1.1.0-rc.2 is 10010003002, got $(num 1.1.0-rc.2)"
[ "$(num 1.1.0)" -gt 2147483647 ] 2>/dev/null || fail "test compares numbers past 2^31"

# Not versions: false, and the shell goes on
for s in none '' x 1 1..2 .1.2 1.2. 1.2.3.4 1.08.0 01.0.0 1.1000.0 1.2.x v1.1.0 \
	1.1.0- -rc.1 '1.1.0 rc.1' '$(reboot)' '1.1.0*'; do
	n=$(num "$s")
	[ "$n" = no ] || fail "'$s' is not a version, got $n"
done

# newer_seed: the image's ZyperOS over the card's only when it is newer.
# SEED_V as post-build.sh writes it, from the image's VERSION.
seed() {
	SEED_V=
	if [ "$1" != none ]; then
		printf '%s\n' "$1" > "$D/seed.VERSION"
		SEED_V=$(. "$D/version_of.sh"; version_of "$D/seed.VERSION" && echo "$v")
	fi
	rm -f "$D/card/ZyperOS/VERSION"
	[ "$2" = none ] || printf '%s\n' "$2" > "$D/card/ZyperOS/VERSION"
	(
		CARD=$D/card
		. "$D/version_of.sh"
		. "$D/newer_seed.sh"
		if newer_seed; then echo copy; else echo keep; fi
	) 2>/dev/null || echo died
}
want() {
	got=$(seed "$2" "$3")
	[ "$got" = "$1" ] || fail "image $2 over card $3: want $1, got $got"
}
want copy 1.1.0-test.1 1.0.6
want copy 1.1.0-rc.1 1.0.6
want copy 1.0.7-kit.1 1.0.6
want copy 1.1.0-rc.1 1.1.0-test.1
want copy 1.1.0-rc.1 1.1.0-rc.0
want copy 1.1.0-rc.2 1.1.0-rc.1
want copy 1.1.0 1.1.0-rc.2
want copy 1.1.0 1.0.6
want copy 1.1.0-rc.1 none
want copy 1.1.0-rc.1 x
want keep 1.1.0-rc.1 1.1.0-rc.1
want keep 1.1.0-rc.1 1.1.0
want keep 1.1.0-test.1 1.1.0-rc.1
want keep 1.0.6 1.1.0-rc.1
want keep none 1.0.6
want keep x 1.0.6

if [ "$fails" -gt 0 ]; then
	echo "check-seed-version: $fails failed"
	exit 1
fi
echo "ok: a version with a suffix is a version at boot, below its release and in order, and nothing else is"
