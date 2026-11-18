#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# launch.sh's start on a card a power cut broke, on both devices: a copy of
# the card script (sd/zyperos/launch.sh) and of each device's device.sh beside
# it, with the card's paths and /tmp moved under a scratch folder and the
# device's commands stubbed (poweroff, reboot, pidof), run by sh.
#
#   1. boot.env is junk (what a power cut can leave): read, not sourced, so
#      the script goes on to the launcher, where a syntax error in a sourced
#      file ended it before ZyperOS (2026-11-15).
#   2. the launcher stops at once, five times: launch.sh is set aside, so the
#      next start copies ZyperOS again (plastron's zyperos.init), a marker
#      says so, and the handheld goes off.
#   3. it does again after that copy: left as it is, and off.
#   4. a power-off asked for (the launcher's /tmp/zyperos_poweroff) takes the
#      marker away.
#   5. launch.sh with no device.sh beside it (a file the card check took
#      away): launch.sh is set aside as in 2, and the handheld goes off,
#      where `.` on the missing file would have ended the script before
#      ZyperOS.
#   6. on the Pixel 2, the kernel's line about the battery's level at the
#      start (plastron's 9-px2-rk817-start-soc.patch) is in the start's log.
D=${TMPDIR:-/tmp}/zyperos-launch-check.$$
trap 'rm -rf "$D"' EXIT
fails=0
fail() { echo "FAIL $dev: $*"; fails=$((fails + 1)); }

# The copies of launch.sh and device.sh, and a check that each edit found its
# line in launch.sh: an edit that finds nothing would test the device's paths
# instead. device.sh names the card only through launch.sh's SDCARD.
edit() {
	sed -e "s#/mnt/SDCARD#$D/card#g" -e "s#/usr/share/zyperos/card#$D/seed#g" \
	    -e "s#/tmp/#$D/tmp/#g" -e "s#/dev/kmsg#$D/kmsg#g" -e 's#( sleep 8$#( exit#' "$1"
}
prepare() {
	rm -rf "$D"
	mkdir -p "$D/card/ZyperOS" "$D/card/.userdata/$dev" "$D/seed/ZyperOS" "$D/bin" "$D/tmp"
	for p in '/mnt/SDCARD' '/usr/share/zyperos/card' '( sleep 8$' '/dev/kmsg' \
	         '^device_radios &$'; do
		grep -q -- "$p" sd/zyperos/launch.sh || { fail "launch.sh no longer has '$p'"; return 1; }
	done
	edit sd/zyperos/launch.sh > "$D/card/ZyperOS/launch.sh"
	# The radios: none here. The Pixel 2's Wi-Fi follower loops for good in
	# the background; the Brick's radios start only with boot.env saying so,
	# which junk does not
	{ edit "sd/$dev/device.sh"; echo 'device_radios() { :; }'; } > "$D/card/ZyperOS/device.sh"
	cp "$D/card/ZyperOS/launch.sh" "$D/card/ZyperOS/device.sh" "$D/seed/ZyperOS/"
	for c in poweroff reboot; do
		printf '#!/bin/sh\necho %s >> "%s/calls"\n' "$c" "$D" > "$D/bin/$c"
	done
	printf '#!/bin/sh\nexit 1\n' > "$D/bin/pidof"
	# The kernel's log, with the gauge's line among others
	printf '#!/bin/sh\necho "[    0.5] mmc0: new card"\necho "[    1.2] %s"\necho "[    1.3] usb 1-1: new device"\n' \
	       'rk817-charger rk817-charger: battery at start: 49% from the voltage; off 120 min, on 0x80, sampled 3815 mV at -250 mA, at rest 3843 mV (49%), counter 85%' \
	       > "$D/bin/dmesg"
	printf '#!/bin/sh\nexit 0\n' > "$D/card/ZyperOS/diatom"
	chmod +x "$D"/bin/* "$D/card/ZyperOS/diatom"
}

# The launcher: $1 is what it does, "stop" (at once, an error) or "off" (a
# power-off asked for)
launcher() {
	if [ "$1" = off ]; then
		printf '#!/bin/sh\n: > "%s/tmp/zyperos_poweroff"\nexit 0\n' "$D" > "$D/card/ZyperOS/zyperos.elf"
	else
		printf '#!/bin/sh\necho "the launcher stops"\nexit 1\n' > "$D/card/ZyperOS/zyperos.elf"
	fi
	chmod +x "$D/card/ZyperOS/zyperos.elf"
}

run() {
	rm -f "$D/calls"
	PATH="$D/bin:$PATH" sh "$D/card/ZyperOS/launch.sh" < /dev/null > "$D/out" 2>&1
	LOG=$D/card/.userdata/$dev/logs/zyperos.log
}

for dev in pixel2 brick; do
	prepare || continue
	U=$D/card/.userdata/$dev
	printf "WIFI='1\nBLU\001\377'\"\`)(" > "$U/boot.env"
	launcher stop
	run
	grep -q 'the launcher stops' "$LOG" 2>/dev/null ||
		fail "the launcher never ran with a broken boot.env: $(tail -n 3 "$D/out")"
	[ "$(grep -c 'the launcher stops' "$LOG" 2>/dev/null)" = 5 ] ||
		fail "the launcher ran $(grep -c 'the launcher stops' "$LOG" 2>/dev/null) times, not 5"
	[ ! -f "$D/card/ZyperOS/launch.sh" ] && [ -f "$D/card/ZyperOS/launch.sh.failed" ] ||
		fail "launch.sh was not set aside after five fast stops"
	[ -f "$U/start-failed" ] || fail "no start-failed marker"
	grep -q 'copies ZyperOS onto the card again' "$LOG" || fail "the log does not say a copy comes"
	grep -q poweroff "$D/calls" 2>/dev/null || fail "no power-off after five fast stops"
	# The battery's line, copied in the background: a moment for it
	if [ "$dev" = pixel2 ]; then
		i=0
		while [ $i -lt 20 ] && ! grep -q '^kernel: battery at start: ' "$LOG"; do
			sleep 0.1
			i=$((i + 1))
		done
		[ "$(grep -c '^kernel: battery at start: 49% from the voltage; off 120 min, on 0x80, sampled 3815 mV at -250 mA, at rest 3843 mV (49%), counter 85%$' "$LOG")" = 1 ] ||
			fail "the start's log has not the kernel's battery line once: $(grep battery "$LOG")"
	fi

	# The next start's copy, as zyperos.init makes it: launch.sh back
	cp "$D/seed/ZyperOS/launch.sh" "$D/card/ZyperOS/launch.sh"
	run
	[ -f "$D/card/ZyperOS/launch.sh" ] || fail "launch.sh set aside a second time"
	grep -q 'after a fresh copy too; left as it is' "$LOG" || fail "the second time is not said"
	grep -q poweroff "$D/calls" 2>/dev/null || fail "no power-off the second time"

	launcher off
	run
	[ ! -f "$U/start-failed" ] || fail "a power-off asked for left the marker"

	# device.sh gone: launch.sh set aside for the next start's copy, and off,
	# with the launcher never started
	rm -f "$D/card/ZyperOS/device.sh" "$D/card/ZyperOS/launch.sh.failed" "$LOG"
	launcher stop
	run
	[ ! -f "$D/card/ZyperOS/launch.sh" ] && [ -f "$D/card/ZyperOS/launch.sh.failed" ] ||
		fail "launch.sh was not set aside with no device.sh: $(tail -n 3 "$D/out")"
	grep -q 'the next start copies ZyperOS' "$D/kmsg" 2> /dev/null ||
		fail "the kernel's log does not say a copy comes with no device.sh"
	grep -q poweroff "$D/calls" 2>/dev/null || fail "no power-off with no device.sh"
	! grep -q 'the launcher stops' "$LOG" 2>/dev/null ||
		fail "the launcher ran with no device.sh"
	[ "$fails" = 0 ] && echo "ok $dev: a junk boot.env read as off, five fast stops or no device.sh set launch.sh aside once"
done
[ "$fails" = 0 ] || exit 1
