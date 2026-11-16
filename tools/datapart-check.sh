#!/bin/sh
# make check-datapart: where the Brick's first start puts the games partition
# (plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/datapart,
# games_place), over a made-up /sys: right after root B on a 4 MiB boundary,
# as partition 5; nothing on a card without root B; a games partition that is
# there already found by its name, so nothing is made over it.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/datapart-check.XXXXXX")
fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# part N NAME START SIZE: a partition in the made-up /sys
part() {
	d=$T/sys/class/block/mmcblk0p$1
	mkdir -p "$d"
	printf 'MAJOR=179\nPARTN=%s\nPARTNAME=%s\n' "$1" "$2" > "$d/uevent"
	echo "$3" > "$d/start"
	echo "$4" > "$d/size"
	echo "$1" > "$d/partition"
}
place() { (DATAPART_TEST=1 SYS=$T/sys KMSG=$T/kmsg; . "$ROOT/plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/datapart"; games_place); }
found() { (DATAPART_TEST=1 SYS=$T/sys; . "$ROOT/plastron/buildroot/board/brick/rootfs-overlay/etc/init.d/datapart"; part "$1"); }

# The image's four: boot at 4 MiB, root A 512 MB, state 16 MB, root B 512 MB
part 1 boot 8192 131072
part 2 plastron-root 139264 1048576
part 3 plastron-state 1187840 32768
part 4 plastron-root-b 1220608 1048576
[ "$(place)" = "2269184 5" ] && ok "after root B, on a 4 MiB boundary, partition 5" ||
	bad "after root B: $(place)"

# Root B ending off a boundary: the games partition starts at the next one
echo 1048000 > "$T/sys/class/block/mmcblk0p4/size"
[ "$(place)" = "2269184 5" ] && ok "rounded up to the next 4 MiB" || bad "rounding: $(place)"

# A games partition already there is found by its name, and start() never
# calls games_create then
part 5 zyperos 2269184 60000000
[ "$(found zyperos)" = "/dev/mmcblk0p5" ] && ok "the games partition found by its name" ||
	bad "games partition: $(found zyperos)"

# A card without root B (another layout): no place, said on the log
rm -r "$T/sys/class/block/mmcblk0p4" "$T/sys/class/block/mmcblk0p5"
[ -z "$(place)" ] && grep -q "no root B" "$T/kmsg" && ok "no root B: no games partition, and why" ||
	bad "no root B: '$(place)'"

rm -rf "$T"
if [ $fails -gt 0 ]; then
	echo "check-datapart: $fails failed"
	exit 1
fi
echo "check-datapart: all passed"
