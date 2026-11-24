#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# plastron's /usr/sbin/crashlog against a stand-in pstore folder: a panic's
# dump becomes a crash-*.txt report with the kernel's last lines and is let
# go, a console that ends in the kernel's own restart says nothing, one that
# ends some other way (a reset Linux did not ask for) is a report too, the
# RCU stall panic is set at every start, and only the newest CRASH_KEEP
# reports stay. And the facts beside each report, for ZyperOS's notice
# (LASTBOOT_RUN): the reason (each panic's kind, rootwait, a stuck task,
# ZyperOS's watchdog, the hardware watchdog's reset during a start) and the
# start's last step. The boot partition is the device's, so not here. Runs with
# the computer's sh; the device's is busybox 1.37's ash.
set -u
CRASHLOG=plastron/buildroot/board/px2/rootfs-overlay/usr/sbin/crashlog
T=$(mktemp -d "${TMPDIR:-/tmp}/crashlog-check.XXXXXX")
trap 'rm -rf "$T"' EXIT
# As root with the right to mount (a cloud VM, a privileged container),
# crashlog's own `mount -t pstore` put the computer's pstore over the
# stand-in folder: nothing could be written in it, and it stayed mounted.
# This mount refuses, as mount does for a user on CI.
mkdir "$T/bin"
printf '#!/bin/sh\nexit 32\n' > "$T/bin/mount"
chmod +x "$T/bin/mount"
PATH=$T/bin:$PATH
fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

run() {
	PSTORE=$T/pstore RCU_PANIC=$T/rcu RUN_REPORT=$T/run-report CRASH_DIR=$T/logs CRASH_KEEP=3 \
		LASTBOOT_RUN=$T/lastboot \
		sh "$CRASHLOG" > "$T/out" 2>&1
}
fresh() {
	rm -rf "$T/pstore" "$T/logs" "$T/run-report" "$T/lastboot"
	mkdir -p "$T/pstore"
	echo 0 > "$T/rcu"
}
reports() { ls "$T/logs" 2> /dev/null | grep -c '^crash-.*\.txt$'; }
# One key of the facts left for ZyperOS
fact() { sed -n "s/^$1=//p" "$T/lastboot" 2> /dev/null; }

echo "nothing kept: nothing said"
fresh
run
[ "$(reports)" = 0 ] && ok "no report" || bad "a report with nothing in pstore"
[ "$(cat "$T/rcu")" = 1 ] && ok "the RCU stall panic is on" || bad "panic_on_rcu_stall is $(cat "$T/rcu")"

echo "a panic"
fresh
printf '<0>[  61.2] watchdog: BUG: soft lockup - CPU#2 stuck for 22s! [zyperos.elf:412]\n<0>[  61.3] Kernel panic - not syncing: softlockup: hung tasks\n' > "$T/pstore/dmesg-ramoops-0"
printf '[  40.0] usb 1-1: new device\r\n[  61.3] Kernel panic - not syncing: softlockup: hung tasks\r\n' > "$T/pstore/console-ramoops-0"
run
[ "$(reports)" = 1 ] && ok "one report" || bad "$(reports) reports for one panic: $(cat "$T/out")"
r=$(ls "$T/logs"/crash-*.txt 2> /dev/null | head -n 1)
grep -q 'stopped with a panic' "$r" 2> /dev/null && grep -q 'soft lockup - CPU#2' "$r" &&
	ok "it says a panic, with the dump" || bad "the report lacks the panic or its dump"
grep -q 'new device' "$r" 2> /dev/null && ok "and the end of the console" || bad "no console in the report"
grep -q "$(printf '\r')" "$r" 2> /dev/null && bad "carriage returns left in the report" || ok "plain line ends"
[ ! -f "$T/pstore/dmesg-ramoops-0" ] && ok "the dump is let go" || bad "the dump is still in pstore"
[ -f "$T/run-report" ] && ok "the report stays for setuplog" || bad "no report left for setuplog"
[ "$(fact reason)" = lockup ] && [ "$(fact stage)" = K1 ] && [ "$(fact kernel_time)" = 61.30 ] &&
	ok "its facts: a lockup, in the kernel, at 61.30 s" ||
	bad "its facts: $(tr '\n' ' ' < "$T/lastboot" 2> /dev/null)"
[ "$(fact last)" = "Kernel panic - not syncing: softlockup: hung tasks" ] && [ "$(fact device)" = px2 ] &&
	[ "$(fact report)" = "${r##*/}" ] && [ -n "$(fact time)" ] && [ "$(fact version)" = - ] &&
	ok "the last line, the device, the report's name, the time" || bad "the facts' other keys: $(tr '\n' ' ' < "$T/lastboot")"
# The start after, as the kernel leaves it: the dump gone, and its console
# that start's own, which ended in a restart asked for
printf '[  90.0] reboot: Restarting system\n' > "$T/pstore/console-ramoops-0"
run
[ "$(reports)" = 1 ] && ok "said once: the next start adds nothing" || bad "the next start said it again"
# Two reports in one second each keep their own file
printf 'Kernel panic\n' > "$T/pstore/dmesg-ramoops-0"
run
printf 'Kernel panic\n' > "$T/pstore/dmesg-ramoops-0"
run
[ "$(reports)" -ge 2 ] && ok "a second report in the same second is a file of its own" ||
	bad "$(reports) reports after two more panics"

echo "a console that ends in the kernel's own restart"
fresh
printf '[ 200.1] zyperos: restart for System Update\n[ 201.0] reboot: Restarting system\n' > "$T/pstore/console-ramoops-0"
run
[ "$(reports)" = 0 ] && ok "no report after a restart asked for" || bad "a report after a normal restart"
printf '[ 300.0] reboot: Power down\n' > "$T/pstore/console-ramoops-0"
run
[ "$(reports)" = 0 ] && ok "nor after a power-off" || bad "a report after a power-off"

echo "a reset without the kernel's restart"
fresh
printf '[  52.7] mmc0: card status 0x900\n[  53.0] rockchip-thermal: thermal shutdown\n' > "$T/pstore/console-ramoops-0"
run
r=$(ls "$T/logs"/crash-*.txt 2> /dev/null | head -n 1)
grep -q 'reset before this start' "$r" 2> /dev/null && grep -q 'thermal shutdown' "$r" &&
	ok "a report of the reset, with the console" || bad "no report of a reset: $(cat "$T/out")"

echo "each reason and step, in the facts left for ZyperOS"
case_of() { # name dump console reason stage heading
	fresh
	[ -n "$2" ] && printf "$2" > "$T/pstore/dmesg-ramoops-0"
	printf "$3" > "$T/pstore/console-ramoops-0"
	run
	r=$(ls "$T/logs"/crash-*.txt 2> /dev/null | head -n 1)
	if [ "$(fact reason)" = "$4" ] && [ "$(fact stage)" = "$5" ] && grep -q "$6" "$r" 2> /dev/null; then
		ok "$1: reason=$4 stage=$5"
	else
		bad "$1: $(tr '\n' ' ' < "$T/lastboot" 2> /dev/null), $(head -n 1 "$r" 2> /dev/null)"
	fi
}
case_of "no root in rootwait's 20 s" \
	'<0>[  20.4] VFS: Cannot open root device "/dev/mmcblk0p2" or unknown-block(0,0): error -6\n<0>[  20.4] Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)\n' \
	'[   0.43] Waiting for root device /dev/mmcblk0p2...\n[  20.4] Kernel panic - not syncing: VFS: Unable to mount root fs\n' \
	rootwait K1 'stopped with a panic'
case_of "a task stuck 60 s in the card's repair" \
	'<3>[ 75.0] INFO: task fsck.exfat:301 blocked for more than 60 seconds.\n<0>[ 75.1] Kernel panic - not syncing: hung_task: blocked tasks\n' \
	'[   0.55]  mmcblk0: p1 p2 p3 p4\n[   1.2] zyboot: stage=C1\n[   1.3] zyboot: stage=C3\n[  75.1] Kernel panic - not syncing: hung_task: blocked tasks\n' \
	hung_task C3 'stopped with a panic'
case_of "an RCU stall" '<0>[ 30.0] Kernel panic - not syncing: RCU Stall\n' '[ 30.0] rcu: INFO: rcu_sched self-detected stall\n' \
	rcu K1 'stopped with a panic'
case_of "a start stopped in the first copy, reset by the hardware watchdog" '' \
	'[   0.55]  mmcblk0: p1 p2 p3 p4\n[   0.57] Run /sbin/init as init process\n[   0.9] hwdog: /dev/watchdog armed: 60 s, petted every 10 s\n[   1.2] zyboot: stage=C1\n[   1.9] zyboot: stage=U2\n[  14.0] zyperos: copying\n' \
	watchdog U2 'while it was starting, at step U2'
case_of "a kernel stopped before init (zytest=hang's)" '' \
	'[   0.55]  mmcblk0: p1 p2 p3 p4\n[   0.57] Run /sbin/init as init process\n[   0.6] hwdog: zytest=hang: init held here\n' \
	watchdog K2 'at step K2'
case_of "ZyperOS's watchdog restarted it" '' \
	'[   1.2] zyboot: stage=C1\n[   1.4] zyboot: stage=L1\n[   3.1] zyperos: first frame at 3.100 s\n[ 900.0] zyperos: watchdog: the launcher'"'"'s loop has not come round for 60 s: the screen off\n[ 960.0] zyperos: watchdog: the loop has not come round for 120 s: an emergency sync, and a restart\n[ 963.0] sysrq: Resetting\n' \
	launcher H "ZyperOS's watchdog restarted"
case_of "a reset after the start" '' \
	'[   1.4] zyboot: stage=L1\n[   3.1] zyperos: first frame at 3.100 s\n[ 500.0] rockchip-thermal: thermal shutdown\n' \
	reset H 'without the kernel'
fresh
printf '[ 200.1] zyperos: first frame\n[ 201.0] reboot: Restarting system\n' > "$T/pstore/console-ramoops-0"
run
[ ! -f "$T/lastboot" ] && ok "no facts after a restart asked for" || bad "facts after a normal restart"

echo "the newest three kept"
fresh
mkdir -p "$T/logs"
for d in 20261101-000000 20261102-000000 20261103-000000 20261104-000000; do
	echo old > "$T/logs/crash-$d.txt"
done
echo "not a report" > "$T/logs/zyperos.log"
printf 'Kernel panic\n' > "$T/pstore/dmesg-ramoops-1"
run
[ "$(reports)" = 3 ] && ok "three reports left" || bad "$(reports) reports left, not 3"
[ ! -f "$T/logs/crash-20261101-000000.txt" ] && [ ! -f "$T/logs/crash-20261102-000000.txt" ] &&
	[ -f "$T/logs/crash-20261104-000000.txt" ] && ok "the oldest went" || bad "the wrong ones went: $(ls "$T/logs")"
[ -f "$T/logs/zyperos.log" ] && ok "other files left alone" || bad "zyperos.log went"

[ "$fails" = 0 ] && echo "ok: crashlog keeps a panic's and a reset's report, once" && exit 0
exit 1
