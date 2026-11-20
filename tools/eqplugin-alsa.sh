#!/bin/sh
# The ports' equalizer through the handheld's own alsa-lib, on this computer:
# the plugin (plastron's zyperos-eq) and a small player built with plastron's
# Pixel 2 SDK, run in its build image against the SDK's sysroot (its glibc,
# its libasound, whose ladspa PCM is built in). The ALSA config is the one
# launch.sh gives a port (sd/zyperos/eq-port.sh: its PCMs, and the hook file
# in its ALSA_CONFIG_PATH that loads them), and the system's is the
# handheld's shape: a file alsa.conf's own hook loads, whose pcm.!default
# goes to the mix, as /etc/asound.conf's does (here through
# $XDG_CONFIG_HOME/alsa/asoundrc, the same hook), its dmix swapped for a
# file, so what the port would have sent to the mix is written down (a WAV,
# in the format the chain settled on): a 1 kHz tone through flat bands, then
# through Voice (+3 dB there). The port's PCMs only win over that default
# when loaded after it; this is what shows they are. Then
# tools/eqchain-check.sh on the same alsa-lib: the chain opened as SDL opens
# it at every rate a program asks, over a stand-in for the dmix. Needs Docker,
# plastron's builder image and an SDK volume (make sdk in plastron); skips
# without them. Not in make check: CI has no SDK.
#
#     sh tools/eqplugin-alsa.sh
set -eu
REPO=$(cd "$(dirname "$0")/.." && pwd)
IMAGE=${PLASTRON_IMAGE:-plastron-builder}
command -v docker > /dev/null || { echo "skip: no Docker"; exit 0; }
docker image inspect "$IMAGE" > /dev/null 2>&1 || { echo "skip: no $IMAGE image"; exit 0; }
sdk=${PX2_SDK:-$(docker volume ls -q --filter label=plastron.sdk=px2 | while read -r v; do
	echo "$(docker volume inspect -f '{{.CreatedAt}}' "$v") $v"; done | sort | tail -1 | cut -d' ' -f2)}
[ -n "$sdk" ] || { echo "skip: no plastron SDK volume"; exit 0; }
echo "eqplugin-alsa: against $sdk"
docker run --rm -v "$sdk:/sdk:ro" -v "$REPO:/zyperos:ro" "$IMAGE" sh -eu -c '
	target=$(sed -n "s/^target=//p" /sdk/share/plastron/sdk)
	CC=/sdk/bin/$target-gcc
	S=/sdk/$target/sysroot
	W=$(mktemp -d)
	P=/zyperos/plastron/buildroot/package/zyperos-eq/src
	# As the package builds it (zyperos-eq.mk)
	$CC -O2 -mcpu=cortex-a35 -fPIC -shared -Wall -Wextra -I/zyperos/src -o $W/zyperos_eq.so $P/zyperos_eq.c -lm
	$CC -O2 -Wall -o $W/play /zyperos/tools/eqplugin-play.c -lasound -lm
	# The system'"'"'s config, as the handheld'"'"'s /etc/asound.conf: its default
	# into the mix, the mix swapped for a file
	mkdir -p $W/xdg/alsa
	cat > $W/xdg/alsa/asoundrc <<CONF
pcm.!default {
	type plug
	slave.pcm "capture"
}
pcm.capture {
	type file
	slave.pcm "null"
	file "$W/out.wav"
	format "wav"
}
CONF
	# launch.sh'"'"'s for a port: its PCMs into that mix, and the hook file
	sh /zyperos/sd/zyperos/eq-port.sh capture $W/zyperos_eq.so > $W/port.conf
	sh /zyperos/sd/zyperos/eq-port.sh --after $W/port.conf > $W/port.hook
	run() {
		XDG_CONFIG_HOME=$W/xdg ZYPEROS_EQ_FILE=$W/eq \
		ALSA_CONFIG_PATH=$S/usr/share/alsa/alsa.conf:$W/port.hook \
			$S/lib/ld-linux-aarch64.so.1 --library-path $S/lib:$S/usr/lib $W/play "$@"
	}
	# The launcher'"'"'s file, flat, then Voice (eqdsp.h'"'"'s eq_share)
	printf "ZEQ1\002\000\000\000\000\000\000\000\000\000\000\000" > $W/eq
	run $W/out.wav flat
	printf "ZEQ1\004\000\000\000\376\000\003\003\000\000\000\000" > $W/eq
	run $W/out.wav voice
	# Flat with the cut set (eqdsp.h, byte 14): your PICO-8 quiet while MP4
	# plays, through the same chain, nothing added to it
	printf "ZEQ1\006\000\000\000\000\000\000\000\000\000\001\000" > $W/eq
	run $W/out.wav cut
	echo "eqplugin-alsa: the port config:"; sed "s/^/  /" $W/port.hook $W/port.conf
	# And the chain opened as SDL opens it at every rate a program asks,
	# over a stand-in for the dmix, on the same alsa-lib (tools/eqchain-check.sh)
	EQCHAIN_SDK=1 CC=$CC ALSA_CONF=$S/usr/share/alsa/alsa.conf \
	RUN="$S/lib/ld-linux-aarch64.so.1 --library-path $S/lib:$S/usr/lib" \
		sh /zyperos/tools/eqchain-check.sh
'
