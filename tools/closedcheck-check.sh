#!/bin/sh
# make check-brick-closed: the Brick's start checking its closed files
# (plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/brick,
# closed_check) against the lists its image carries, over a made-up card: all
# there passes; one missing, one cut short, an empty folder or no list fails
# and names what is wrong.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/closedcheck.XXXXXX")
fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }
check() { (BRICK_INIT_TEST=1 CLOSED_LISTS=$T/lists CLOSED_DIR=$T/closed; . "$ROOT/plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/brick"; closed_check); }

mkdir -p "$T/lists" "$T/closed"
printf 'egl' > "$T/closed/libEGL.so"
printf 'firmware' > "$T/closed/rgx.fw.22.102.54.38"
printf 'wifi!' > "$T/closed/fw_xr829.bin"
printf '3 libEGL.so\n8 rgx.fw.22.102.54.38\n' > "$T/lists/pvr-ge8300.list"
printf '5 fw_xr829.bin\n' > "$T/lists/xr829-firmware.list"

out=$(check) && [ -z "$out" ] && ok "all there: it passes, quietly" || bad "all there: '$out'"

printf 'fir' > "$T/closed/rgx.fw.22.102.54.38"
out=$(check); [ $? -ne 0 ] && [ "$out" = "rgx.fw.22.102.54.38" ] && ok "one cut short: it fails, naming it" ||
	bad "cut short: '$out'"
printf 'firmware' > "$T/closed/rgx.fw.22.102.54.38"

rm "$T/closed/fw_xr829.bin"
out=$(check); [ $? -ne 0 ] && [ "$out" = "fw_xr829.bin" ] && ok "one missing: it fails, naming it" ||
	bad "missing: '$out'"

rm -r "$T/closed"
out=$(check); [ $? -ne 0 ] && [ "$out" = "libEGL.so" ] && ok "no folder: it fails at the first file" ||
	bad "no folder: '$out'"

rm "$T/lists/"*.list
out=$(check); [ $? -ne 0 ] && [ "$out" = "(no list of them)" ] && ok "no lists in the image: it fails, and says so" ||
	bad "no lists: '$out'"

rm -rf "$T"
if [ $fails -gt 0 ]; then
	echo "check-brick-closed: $fails failed"
	exit 1
fi
echo "check-brick-closed: all passed"
