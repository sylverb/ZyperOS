#!/bin/sh
# The handhelds a ZyperOS release carries (config/devices), and the release
# notes' word that each one passed its device test.
#
#   tools/release-devices.sh list [FILE]
#       the device ids in config/devices (or FILE), one a line, in its order;
#       fails on a line that is not one id, an id twice, or no id at all
#   tools/release-devices.sh tested NOTES [FILE]
#       whether the release notes NOTES have a line
#
#           Tested on <id>: <who tested it, and on what>
#
#       for every device in config/devices (or FILE): from the line's start,
#       "Tested on", the id as config/devices has it, a colon and a space,
#       then some words. A line that starts "Tested on" in any other way (a
#       list mark before it, another case, no colon, nothing after it) or
#       names a device that is not listed fails it too. Says what is wrong
#       and exits 1. tools/release.sh publishes nothing without it
#   tools/release-devices.sh check
#       make check-release-devices: `tested` and `list` on sample notes and
#       lists, and each listed device's src/device/<id>_profile.c and
#       res/update/min-system-<id>, which tools/update-pack.py reads
#
# POSIX sh and awk: tools/release.sh runs it on the Mac, and the release and
# Pages workflows on GitHub's Ubuntu.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEVICES=$ROOT/config/devices

die() { echo "release-devices: $*" >&2; exit 1; }

# list FILE: its ids. A comment runs from # to the end of its line
list() {
	[ -f "$1" ] || die "no $1"
	awk -v file="$1" '
		{ sub(/#.*/, ""); gsub(/^[ \t]+|[ \t\r]+$/, "") }
		$0 == "" { next }
		$0 !~ /^[a-z0-9]+$/ {
			printf "release-devices: %s:%d: \"%s\" is not one device id (a-z, 0-9)\n", file, NR, $0 > "/dev/stderr"
			bad = 1; next
		}
		seen[$0]++ { printf "release-devices: %s:%d: %s is listed twice\n", file, NR, $0 > "/dev/stderr"; bad = 1; next }
		{ print; n++ }
		END {
			if (!bad && !n) { printf "release-devices: %s lists no device\n", file > "/dev/stderr"; bad = 1 }
			exit bad
		}' "$1"
}

# tested NOTES FILE: a Tested on line for each device FILE lists
tested() {
	[ -f "$1" ] || die "no release notes at $1"
	ids=$(list "$2") || exit 1
	awk -v ids="$(echo $ids)" -v notes="$1" -v file="$2" '
		BEGIN { n = split(ids, want, " "); for (i = 1; i <= n; i++) listed[want[i]] = 1 }
		{
			line = $0
			sub(/\r$/, "", line)
			# Meant as one: "Tested on" in any case, after spaces and a list
			# or quote mark at most. Prose that says "tested on" later in a
			# line is not one
			bare = tolower(line)
			gsub(/[ \t]+/, " ", bare)
			sub(/^ ?([-*+>]|[0-9]+[.)])? ?/, "", bare)
			if (index(bare, "tested on") != 1) next
			if (line !~ /^Tested on [a-z0-9]+: [^ \t]/) {
				printf "release-devices: %s:%d: not in the form \"Tested on <id>: <who, on what>\": %s\n", notes, NR, line > "/dev/stderr"
				bad = 1; next
			}
			id = substr(line, 11)
			id = substr(id, 1, index(id, ":") - 1)
			if (!(id in listed)) {
				printf "release-devices: %s:%d: %s is not a device in %s (%s)\n", notes, NR, id, file, ids > "/dev/stderr"
				bad = 1; next
			}
			done[id] = 1
		}
		END {
			for (i = 1; i <= n; i++) if (!(want[i] in done)) {
				printf "release-devices: %s has no \"Tested on %s: <who, on what>\" line: %s has not passed its device test for this version\n", notes, want[i], want[i] > "/dev/stderr"
				bad = 1
			}
			exit bad
		}' "$1"
}

check() {
	T=$(mktemp -d)
	trap 'rm -rf "$T"' EXIT
	fails=0
	fail() { echo "check-release-devices: FAIL $*" >&2; fails=$((fails + 1)); }
	# pass WHAT NOTES-TEXT [FILE] / refuse WHAT NOTES-TEXT WORD [FILE]: the
	# notes pass, or fail with WORD in what is said
	pass() {
		printf '%s\n' "$2" > "$T/notes.md"
		(tested "$T/notes.md" "${3:-$T/devices}") > /dev/null 2> "$T/err" || fail "$1: $(cat "$T/err")"
	}
	refuse() {
		printf '%s\n' "$2" > "$T/notes.md"
		if (tested "$T/notes.md" "${4:-$T/devices}") > /dev/null 2> "$T/err"; then
			fail "$1: passed"
		elif ! grep -q -- "$3" "$T/err"; then
			fail "$1: said $(cat "$T/err"), not $3"
		fi
	}
	printf '# Two devices, as when a second one is released\npixel2\nbrick   # with a comment\n\n' > "$T/devices"
	HEAD='ZyperOS 9.9.9.

## What'"'"'s new

- Everything here was tested on a Pixel 2 and on a Brick, which is prose.
- **Tested on more cards** is a point, not a Tested line.

## Tested'
	PX='Tested on pixel2: Tarik, the release image on two Pixel 2s'
	BR='Tested on brick: Bruno, the release image on a Brick Hammer'

	pass "both devices" "$HEAD

$PX

$BR"
	pass "in either order, no blank line between" "$HEAD
$BR
$PX"
	pass "a device tested twice" "$HEAD
$PX
Tested on pixel2: Bruno, a Pixel 2 with an old card
$BR"
	refuse "a device missing" "$HEAD
$PX" "no \"Tested on brick"
	refuse "no Tested line at all" "$HEAD" "Tested on pixel2"
	refuse "a misspelled id" "$HEAD
Tested on pixl2: Tarik
$BR" "pixl2 is not a device"
	refuse "a device not listed" "$HEAD
$PX
$BR
Tested on brickpro: Bruno" "brickpro is not a device"
	for wrong in "Tested on Pixel 2: Tarik" "Tested on Pixel2: Tarik" "tested on pixel2: Tarik" "Tested on pixel2:" \
		"Tested on pixel2:Tarik" "Tested on pixel2 - Tarik" "Tested on pixel2" "- Tested on pixel2: Tarik" \
		"  Tested on pixel2: Tarik" "> Tested on pixel2: Tarik" "1. Tested on pixel2: Tarik" "Tested  on pixel2: Tarik" \
		"Tested on pixel2:  Tarik"; do
		refuse "the wrong form ($wrong)" "$HEAD
$PX
$wrong
$BR" "not in the form"
	done
	printf '%s\r\n%s\r\n' "$PX" "$BR" > "$T/crlf.md"
	(tested "$T/crlf.md" "$T/devices") 2> "$T/err" || fail "notes with CRLF line ends: $(cat "$T/err")"
	(tested "$T/none.md" "$T/devices") > /dev/null 2>&1 && fail "notes that are not there passed"

	# The lists themselves
	[ "$(list "$T/devices" | tr '\n' ' ')" = "pixel2 brick " ] || fail "the sample list reads $(list "$T/devices" | tr '\n' ' ')"
	for bad in "Pixel2" "pixel2 brick" "pixel-2" "pixel2
pixel2" "# only a comment"; do
		printf '%s\n' "$bad" > "$T/bad"
		(list "$T/bad") > /dev/null 2>&1 && fail "a list of \"$bad\" passed"
	done

	# config/devices: each one with its profile and its min-system, and the
	# notes of a release tested on each passing against it
	ids=$(list "$DEVICES") || { fail "config/devices does not read"; ids=; }
	notes=
	for id in $ids; do
		[ -f "$ROOT/src/device/${id}_profile.c" ] || fail "$id has no src/device/${id}_profile.c"
		m="$ROOT/res/update/min-system-$id"
		if [ ! -f "$m" ]; then
			fail "$id has no res/update/min-system-$id"
		elif ! grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?' "$m"; then
			fail "res/update/min-system-$id is not one version: $(cat "$m")"
		fi
		notes="$notes
Tested on $id: Tarik, the release image"
	done
	[ ! -e "$ROOT/res/update/min-system" ] || fail "res/update/min-system is back: it is res/update/min-system-<id>, one a device"
	pass "a release tested on every device in config/devices" "$HEAD$notes" "$DEVICES"
	last=$(printf '%s\n' "$ids" | tail -1)
	[ -z "$last" ] || refuse "config/devices with $last not tested" \
		"$(printf '%s\n' "$HEAD$notes" | grep -v "^Tested on $last: ")" "Tested on $last" "$DEVICES"

	# tools/update-pack.py takes the device's min-system when none is given,
	# and stops for a device with none
	mkdir -p "$T/payload/ZyperOS"
	echo hello > "$T/payload/ZyperOS/hello.txt"
	for id in $ids; do
		[ -f "$ROOT/res/update/min-system-$id" ] || continue
		rm -rf "$T/out"
		if python3 "$ROOT/tools/update-pack.py" --payload "$T/payload/ZyperOS" --version 9.9.9 --device "$id" \
			--out "$T/out" > /dev/null 2> "$T/err"; then
			grep -qx "min-system $(cat "$ROOT/res/update/min-system-$id")" "$T/out/ZyperOS-$id-update.txt.unsigned" ||
				fail "update-pack.py's list for $id has not its min-system: $(grep '^min-system' "$T/out/ZyperOS-$id-update.txt.unsigned")"
		else
			fail "update-pack.py for $id: $(cat "$T/err")"
		fi
	done
	python3 "$ROOT/tools/update-pack.py" --payload "$T/payload/ZyperOS" --version 9.9.9 --device nosuchdevice \
		--out "$T/out" > /dev/null 2>&1 && fail "update-pack.py packed for a device with no min-system"

	if [ $fails = 0 ]; then
		echo "check-release-devices: $(echo $ids) in config/devices, each with its profile and min-system; the Tested on lines read"
	else
		echo "check-release-devices: $fails failed" >&2
		exit 1
	fi
}

case "${1:-}" in
list) list "${2:-$DEVICES}" ;;
tested) [ $# -ge 2 ] || die "usage: tools/release-devices.sh tested NOTES [FILE]"; tested "$2" "${3:-$DEVICES}" ;;
check) check ;;
*) echo "usage: tools/release-devices.sh list [FILE] | tested NOTES [FILE] | check" >&2; exit 2 ;;
esac
