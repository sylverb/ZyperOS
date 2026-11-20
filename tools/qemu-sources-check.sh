#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Whether a release's QEMU image has the source of every package in it, in
# one of the release's two sources tars (tools/release.sh, before it writes
# SHA256SUMS):
#
#   sh tools/qemu-sources-check.sh ZyperOS-v<V>-qemu-sources.tar ZyperOS-v<V>-pixel2-sources.tar
#
# The QEMU sources tar (plastron's scripts/qemu-sources.sh) must hold px2sim's
# source, license and Buildroot package, and the QEMU kernel's and U-Boot's
# configuration. Then each package in its manifest.csv (every package in the
# image) passes when the Pixel 2's sources tar has it at the same version, or
# the QEMU sources tar has its source (qemu-sources/<package>-<version>/), or
# its license asks for no source (none of GPL, LGPL, AGPL, MPL, EPL, CDDL,
# EUPL, OSL in it: LLVM's Apache-2.0, say). Any other is named, and the
# check fails: a package only the QEMU image has, under a copyleft license,
# needs its source added to scripts/qemu-sources.sh.
set -eu
Q=$1
P=$2
fail() { echo "qemu-sources-check: $*" >&2; exit 1; }
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT

tar -tf "$Q" > "$T/names" 2> /dev/null || fail "$Q is not a tar"
for f in README.txt manifest.csv plastron/package/px2sim/px2sim.mk plastron/package/px2sim/src/px2sim.c \
	plastron/package/px2sim/src/kmod/px2sim_hw.c plastron/board/qemu/linux.config \
	plastron/board/qemu/u-boot.fragment plastron/configs/qemu_defconfig; do
	grep -qx "qemu-sources/$f" "$T/names" || fail "$Q has no qemu-sources/$f"
done
grep -q '^qemu-sources/px2sim-[^/]*/px2sim-[^/]*\.tar\.gz$' "$T/names" || fail "$Q has no px2sim source archive"
grep -q '^qemu-sources/px2sim-[^/]*/LICENSE$' "$T/names" || fail "$Q has no px2sim license"

tar -xOf "$Q" qemu-sources/manifest.csv > "$T/qemu.csv" 2> /dev/null || fail "no manifest.csv in $Q"
tar -xOf "$P" legal-info/manifest.csv > "$T/pixel2.csv" 2> /dev/null || fail "no legal-info/manifest.csv in $P"
# A manifest's packages, one a line: package, version and license, tab
# between (each field is quoted, and a license can have a comma in it)
rows() { sed '1d; s/^"//; s/"$//' "$1" | awk -F'","' '{ print $1 "\t" $2 "\t" $3 }'; }
rows "$T/pixel2.csv" | cut -f1,2 > "$T/pixel2"
[ -s "$T/pixel2" ] || fail "$P's manifest lists no package"
TAB=$(printf '\t')
n=0 px2=0 here=0 free=0
: > "$T/missing"
rows "$T/qemu.csv" > "$T/qemu"
while IFS=$TAB read -r pkg ver lic; do
	n=$((n + 1))
	if grep -qxF "$pkg$TAB$ver" "$T/pixel2"; then
		px2=$((px2 + 1))
	elif grep -q "^qemu-sources/$pkg-$ver/" "$T/names"; then
		here=$((here + 1))
	elif ! printf '%s\n' "$lic" | grep -Eq 'GPL|MPL|EPL|CDDL|EUPL|OSL'; then
		free=$((free + 1))
	else
		printf '  %s %s (%s)\n' "$pkg" "$ver" "$lic" >> "$T/missing"
	fi
done < "$T/qemu"
[ $n -gt 0 ] || fail "$Q's manifest lists no package"
if [ -s "$T/missing" ]; then
	echo "qemu-sources-check: the QEMU image has these under a copyleft license, with their source in neither tar:" >&2
	cat "$T/missing" >&2
	exit 1
fi
echo "qemu-sources-check: $n packages: $px2 with the Pixel 2's sources, $here in the QEMU sources, $free under licenses that ask for no source"
