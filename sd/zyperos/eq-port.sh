#!/bin/sh
# The equalizer over a port's sound (DESIGN.md 11.60). launch.sh runs it
# before each port:
#
#   eq-port.sh --on            exit 0 when the launcher's file is there and
#                              whole (/run/zyperos/eq: src/eqdsp.h's eq_share,
#                              "ZEQ1", a count, five signed bytes, the boost)
#   eq-port.sh SLAVE [PLUGIN]  the port's own PCMs, on stdout
#   eq-port.sh --after FILE    the file for the port's ALSA_CONFIG_PATH, on
#                              stdout: a hook that loads FILE (the PCMs)
#
# The config's "default" is plug -> alsa-lib's ladspa PCM, running ZyperOS's
# plugin (plastron's zyperos-eq, PLUGIN) -> plug -> SLAVE, the dmix every
# other program mixes into ("mixed" on the Pixel 2, its ipc_key the same), so
# the port still plays over MP4 and under the volume bar as before. The
# plugin reads the bands from the same file once a period, so a change in
# the port's in-game menu reaches it.
#
# Why a hook: alsa-lib reads every file in ALSA_CONFIG_PATH first and runs
# the hooks after, 0, 1, ... in order. alsa.conf's one hook, 0, loads
# /etc/asound.conf, whose pcm.!default would replace the port's (the image
# check for this found the port playing straight into the mix). So the file
# in the port's ALSA_CONFIG_PATH, after alsa.conf, only adds hook 1, which
# loads the PCMs after /etc/asound.conf. errors false: a PCM file that cannot
# be read leaves the port on the system's default, never without sound.
#
# On for every port since the volume boost (src/boost.h, DESIGN.md 11.6):
# its gain is in the same file, portkeys writes it as the keys open the boost
# during the port, and a plugin has to be in the chain from the port's start
# to apply it. Flat and with no boost the plugin passes the sound as it came,
# a copy a period. Only with no file (no launcher ran this boot) does a port
# start with none of this, as it did before the equalizer.
FILE=${ZYPEROS_EQ_FILE:-/run/zyperos/eq}

if [ "${1:-}" = --on ]; then
	[ -f "$FILE" ] && [ "$(head -c 4 "$FILE" 2> /dev/null)" = ZEQ1 ] && exit 0
	exit 1
fi

if [ "${1:-}" = --after ]; then
	cat <<CONF
# The equalizer over this port's sound (ZyperOS's sd/zyperos/eq-port.sh):
# its PCMs, loaded after alsa.conf's own hook (0, /etc/asound.conf)
@hooks.1 {
	func load
	files [ "${2:?eq-port.sh --after: which file}" ]
	errors false
}
CONF
	exit 0
fi

slave=${1:?eq-port.sh: which PCM the port mixes into}
plugin=${2:-/usr/lib/ladspa/zyperos_eq.so}
cat <<CONF
# The equalizer over this port's sound (ZyperOS's sd/zyperos/eq-port.sh)
pcm.!default {
	type plug
	slave.pcm "zyperos_eq"
}

pcm.zyperos_eq {
	type ladspa
	slave.pcm "plug:$slave"
	path "${plugin%/*}"
	plugins [ {
		label zyperos_eq
		filename "$plugin"
		policy none
		input { bindings { 0 "Input L" 1 "Input R" } }
		output { bindings { 0 "Output L" 1 "Output R" } }
	} ]
}
CONF
