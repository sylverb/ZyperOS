#!/bin/sh
# The GKD Pixel 2's part of ZyperOS's card script: sourced by launch.sh
# (sd/zyperos/launch.sh) before anything else, never run. mk/payload.sh puts
# it beside launch.sh as ZyperOS/device.sh.
#
# What the Pixel doesn't have, beside the Brick: Bluetooth, and a radio of its
# own (its Wi-Fi is a USB adapter, when one is plugged in). And every feature
# launch.sh has is on here: plastron's ZyperOS image and this payload carry
# each program they need.

export USERDATA_PATH=$SDCARD/.userdata/pixel2

HAS_PORTS=1
HAS_PORTMASTER=1
HAS_PORTKEYS=1
HAS_PORT_STILL=1
HAS_PORT_TURN=1
HAS_ZYHOME=1
HAS_USBDRIVE=1
HAS_OFF_PENDING=1
# The equalizer over a port's sound runs into the RK817's dmix, the one
# every program mixes into (plastron's /etc/asound.conf); and your PICO-8's,
# with a USB-C DAC in, into the DAC's own chain there, "usb" (its software
# volume, its dmix), as Diatom's and MP4's sound go, once it opens there as
# PICO-8 opens it (launch.sh's run_port)
EQ_SLAVE=mixed
EQ_SLAVE_USB=usb

# The boot partition, read-only, for what ZyperOS Installer left there for a
# card's first start (launch.sh: your PICO-8)
device_boot_mount() {
	mkdir -p /tmp/zyperos-boot &&
		mount -t vfat -o ro,noatime,nosuid,nodev /dev/mmcblk0p1 /tmp/zyperos-boot 2> /dev/null &&
		BOOT_DIR=/tmp/zyperos-boot
}
device_boot_unmount() { umount /tmp/zyperos-boot 2> /dev/null; }

WIFI_DOWN="no adapter, or it did not come up"

# Wi-Fi follows the adapter as it comes and goes: it isn't the Pixel's own, so
# it can be plugged in or pulled at any time, a game included. Looked for every
# 2 seconds with the shell's own test, no process started for it. When it
# appears, and Wi-Fi was left on (boot.env, read again, as ZyperOS rewrites it
# when the setting changes), the same as at boot; when it goes, the
# supplicant and the DHCP client stop, so the next one starts clean. From when
# the splash ends, as on the Brick, so the adapter's driver and the supplicant
# stay out of ZyperOS's way to its first frame; a splash that never ends holds
# it 20 s at most.
wifi_follow() {
	i=0
	while [ $i -lt 200 ] && pidof splash > /dev/null; do
		sleep 0.1
		i=$((i + 1))
	done
	had=
	while : ; do
		if [ -d /sys/class/net/wlan0 ]; then
			if [ -z "$had" ]; then
				had=1
				boot_env
				[ "$WIFI" = 1 ] && wifi_on &
			fi
		elif [ -n "$had" ]; then
			had=
			/etc/init.d/wpa_supplicant stop > /dev/null 2>&1
		fi
		sleep 2
	done
}
device_radios() { wifi_follow; }

# The battery's level at this start, and what it came from: plastron's
# kernel says it in one line as the gauge starts (its
# 9-px2-rk817-start-soc.patch). The line is copied into this start's log
# while the kernel's log, 16 KiB, still has it, so a report of a level that
# jumped after a power-off shows why. In the background: nothing waits on it.
device_log() {
	dmesg 2> /dev/null | sed -n 's/.*\(battery at start: .*\)/kernel: \1/p' >> "$LOG" &
}
