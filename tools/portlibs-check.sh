#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# make check-portlibs: the libraries PortMaster's ports and their runtimes
# load from the system (plastron's buildroot/board/zyperos/ports-libs.txt)
# are asked for in the image of every device whose ZyperOS runs ports.
#
# A library a port loads from the system and the image lacks stops the port
# at its start: every Westonpack port (PortMaster's Weston and Xwayland
# runtime, weston_pkg_0.2) did, "wp_weston: error while loading shared
# libraries: libevdev.so.2", on 1.1.0-rc.6. So, for each device with
# HAS_PORTS=1 in its sd/<device>/device.sh (the Pixel 2; the Brick has no
# bash yet), each line's option is set in the device's Buildroot config (the
# Pixel 2's ZyperOS image is px2_defconfig with board/zyperos/zyperos.config
# merged in, as plastron's scripts/build.sh does), and each line's package is
# in the device's package list (board/<board>/third-party-packages.md, which
# plastron's make release holds to the build's own manifest). A line whose
# option is "-" is a library another package brings in; its package must
# still be listed.
#
# Then plastron's scripts/ports-libs.sh, which scripts/release.sh runs over
# the built image, over roots made here: every library there (files, and
# relative links as Buildroot makes them, in /usr/lib or /lib) passes; one
# missing, or a link to nothing, fails and names it; an empty list or no
# folder is an error, not a pass. No image is built: plastron's release
# checks the real one.
set -u

P=plastron/buildroot
LIST=$P/board/zyperos/ports-libs.txt
LIBS=plastron/scripts/ports-libs.sh
D=build-native/portlibs-check
fails=0
fail() { echo "  FAIL $*"; fails=$((fails + 1)); }
ok() { echo "  ok   $*"; }

[ -f "$LIST" ] || { echo "FAIL no $LIST"; exit 1; }

# The list itself: four fields or more, a soname, an option or "-", once each
lines=$(grep -v '^[[:space:]]*#' "$LIST" | grep -c '[^[:space:]]')
[ "$lines" -gt 0 ] || fail "$LIST lists no library"
mkdir -p build-native
grep -v '^[[:space:]]*#' "$LIST" | while read -r so pkg opt who; do
	[ -n "$so" ] || continue
	case $so in lib*.so | lib*.so.*) ;; *) echo "  FAIL '$so' in $LIST is no soname" ;; esac
	case $opt in - | BR2_PACKAGE_*) ;; *) echo "  FAIL $so: '$opt' is neither an option nor -" ;; esac
	[ -n "$who" ] || echo "  FAIL $so: no word of what loads it"
done > "$D.list"
if [ -s "$D.list" ]; then
	cat "$D.list"
	fails=$((fails + $(wc -l < "$D.list")))
fi
twice=$(grep -v '^[[:space:]]*#' "$LIST" | awk 'NF { print $1 }' | sort | uniq -d)
[ -z "$twice" ] || fail "listed twice in $LIST: $twice"

# device NAME CONFIGS... -- LIST: each line's option set, its package listed
device() {
	name=$1
	shift
	configs=
	while [ "$1" != -- ]; do configs="$configs $1"; shift; done
	shift
	pkgs=$1
	if ! grep -qx 'HAS_PORTS=1' "sd/$name/device.sh" &&
		! grep -q '^HAS_PORTS=1[[:space:]]' "sd/$name/device.sh"; then
		ok "$name runs no ports (sd/$name/device.sh), so its image is not held to the list"
		return
	fi
	for c in $configs; do [ -f "$c" ] || { fail "$name: no $c"; return; }; done
	[ -f "$pkgs" ] || { fail "$name: no $pkgs"; return; }
	before=$fails
	while read -r so pkg opt who; do
		case $so in '' | '#'*) continue ;; esac
		if [ "$opt" != - ]; then
			# The last word on it wins, as in the merge: zyperos.config's
			# over px2_defconfig's
			# shellcheck disable=SC2086
			last=$(cat $configs | grep -E "^($opt=|# $opt is not set)" | tail -n 1)
			[ "$last" = "$opt=y" ] ||
				fail "$name: $so needs $opt=y in$configs ($who)"
		fi
		grep -q "^| $pkg |" "$pkgs" ||
			fail "$name: $so's package $pkg is not in $pkgs"
	done < "$LIST"
	[ "$fails" -eq "$before" ] && ok "$name: every library the ports load is asked for ($lines), and its package listed"
}

echo "the image of each device that runs ports asks for every library they load"
device pixel2 "$P/configs/px2_defconfig" "$P/board/zyperos/zyperos.config" -- "$P/board/px2/third-party-packages.md"
device brick "$P/configs/brick_defconfig" -- "$P/board/brick/third-party-packages.md"

echo "plastron's scripts/ports-libs.sh over made-up roots"
rm -rf "$D"
mkdir -p "$D/full/usr/lib" "$D/full/lib" || exit 1
# Each one a file, and every other one a relative link to a file, as
# Buildroot installs them; the util-linux one in /lib, as the image has it
i=0
grep -v '^[[:space:]]*#' "$LIST" | awk 'NF { print $1, $2 }' > "$D/names"
while read -r so pkg; do
	i=$((i + 1))
	dir=$D/full/usr/lib
	[ "$pkg" = util-linux ] && dir=$D/full/lib
	if [ $((i % 2)) -eq 0 ]; then
		: > "$dir/$so.real"
		ln -s "$so.real" "$dir/$so"
	else
		: > "$dir/$so"
	fi
done < "$D/names"
if out=$(sh "$LIBS" "$D/full" 2>&1); then
	ok "all there: passes ($out)"
else
	fail "all there, and it failed: $out"
fi

first=$(awk 'NR == 1 { print $1 }' "$D/names")
last=$(awk 'END { print $1 }' "$D/names")
cp -R "$D/full" "$D/one-gone"
rm -f "$D/one-gone/usr/lib/$first" "$D/one-gone/lib/$first"
out=$(sh "$LIBS" "$D/one-gone" 2>&1)
s=$?
if [ "$s" -eq 1 ] && printf '%s\n' "$out" | grep -q "$first is not in the image"; then
	ok "$first gone: fails and names it"
else
	fail "$first gone: exit $s, $out"
fi

cp -R "$D/full" "$D/dangling"
for d in usr/lib lib; do
	if [ -e "$D/dangling/$d/$last" ]; then
		rm -f "$D/dangling/$d/$last" "$D/dangling/$d/$last.real"
		ln -s "$last.gone" "$D/dangling/$d/$last"
	fi
done
out=$(sh "$LIBS" "$D/dangling" 2>&1)
s=$?
if [ "$s" -eq 1 ] && printf '%s\n' "$out" | grep -q "$last is not in the image"; then
	ok "$last a link to nothing: fails and names it"
else
	fail "$last a link to nothing: exit $s, $out"
fi

# The two the Westonpack runtime lacked on 1.1.0-rc.6, the image without
# them: both named, nothing else
cp -R "$D/full" "$D/rc6"
for so in libevdev.so.2 libbz2.so.1.0; do
	grep -q "^$so " "$D/names" || fail "$so is not in $LIST: Westonpack's weston and Xwayland load it"
	rm -f "$D/rc6/usr/lib/$so" "$D/rc6/usr/lib/$so.real"
done
out=$(sh "$LIBS" "$D/rc6" 2>&1)
s=$?
if [ "$s" -eq 1 ] && [ "$(printf '%s\n' "$out" | grep -c 'is not in the image')" -eq 2 ] &&
	printf '%s\n' "$out" | grep -q 'libevdev.so.2 is not in the image' &&
	printf '%s\n' "$out" | grep -q 'libbz2.so.1.0 is not in the image'; then
	ok "an image without libevdev.so.2 and libbz2.so.1.0 (1.1.0-rc.6's): fails on those two"
else
	fail "an image without libevdev.so.2 and libbz2.so.1.0: exit $s, $out"
fi

mkdir -p "$D/empty-list"
printf '# nothing\n' > "$D/empty-list/list.txt"
out=$(PORTS_LIBS=$D/empty-list/list.txt sh "$LIBS" "$D/full" 2>&1)
s=$?
if [ "$s" -eq 2 ]; then ok "a list of no library: an error"; else fail "a list of no library: exit $s, $out"; fi
out=$(sh "$LIBS" "$D/no-such-root" 2>&1)
s=$?
if [ "$s" -eq 2 ]; then ok "no root folder: an error"; else fail "no root folder: exit $s, $out"; fi

rm -rf "$D" "$D.list"
if [ "$fails" -gt 0 ]; then
	echo "check-portlibs: $fails failed"
	exit 1
fi
echo "check-portlibs: ok"
