#!/bin/sh
# Put a built ZyperOS payload on a GKD Pixel 2 over the USB cable, without
# taking the card out: the launcher, Diatom, launch.sh, the cores, cards and
# config, whatever differs from what the card has. Then ZyperOS restarts.
#
#   tools/px2-deploy.sh PAYLOAD_DIR      (the ZyperOS/ folder of a payload, as
#                                         mk/payload.sh lays it out, plastron's
#                                         /usr/share/zyperos/card/ZyperOS, or
#                                         px2-payload --programs' programs alone)
#   tools/px2-deploy.sh --restore        the card as it was before the first
#                                         deploy since the last restore
#   tools/px2-deploy.sh --status         what a restore would put back
#
# tools/px2 deploy and tools/px2 restore are the same under the device's lock
# and its card check: use those when another session may use the device.
#
# Needs a development build of plastron (its SSH key on the card), or
# Developer Mode with a key, and the device at 10.42.0.1 (PX2_HOST: its
# Wi-Fi address with SSH over Wi-Fi on, docs/building.md), on SSH's port 22
# (PX2_PORT: a QEMU VM's
# forwarded port, tools/qemu-run.sh). Each file goes to /tmp on the device first
# and is moved into place only once its checksum matches, so a cable pulled
# halfway leaves the old file, not half a new one. plastron's own
# scripts/px2-push.sh is the same thing for the system partition.
#
# What a deploy replaces is kept first, on the card in /mnt/SDCARD/.px2-deploy
# (outside ZyperOS/, so nothing there reads it): the card's own file the first
# time a deploy replaces it, and the name of each file a deploy added. A
# later deploy keeps what the first one kept, so --restore always goes back
# to the card from before the session's first deploy, and then forgets it.
# PX2_DEPLOY_KEEP=0 keeps nothing (a release's payload meant to stay).
set -eu

HOST=${PX2_HOST:-10.42.0.1}
PORT=${PX2_PORT:-22}
KEY=${PX2_KEY:-$HOME/.ssh/zyperos_pixel2}
# make check-px2's stand-ins (tools/px2-check.sh): ssh and scp, and the card
# as a folder on this computer
SSH_BIN=${PX2_SSH:-ssh}
SCP_BIN=${PX2_SCP:-scp}
CARD=${PX2_CARD:-/mnt/SDCARD}
DTMP=${PX2_DEVICE_TMP:-/tmp/px2-deploy}
DEST=$CARD/ZyperOS
SAVE=$CARD/.px2-deploy
# dropbear makes a new host key at every boot of a read-only root: no key to check
OPTS="-o ConnectTimeout=8 -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"
SSH="$SSH_BIN $OPTS -p $PORT -i $KEY root@$HOST"
SCP="$SCP_BIN -O -q $OPTS -P $PORT -i $KEY"

usage() { sed -n '6,13p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2; }
[ $# -eq 1 ] || usage

# ZyperOS starts again with what is on the card now. A new launch.sh only
# runs from the next boot: the shell running the old one keeps reading the
# file it opened. Its device.sh the same: launch.sh sourced it once, at its
# start. Anything else: the launcher restarts itself when it exits
# (launch.sh), and Diatom with it, so a new diatom or core is the one that
# runs. $1: the files that changed, a name a line.
restart() {
	if printf '%s\n' "$1" | grep -qx -e launch.sh -e device.sh; then
		$SSH 'reboot' < /dev/null || true
		echo "px2-deploy: done; launch.sh or device.sh changed, so the device is restarting"
	else
		$SSH 'kill $(pidof diatom) 2> /dev/null; kill $(pidof zyperos.elf) 2> /dev/null; true' < /dev/null
		echo "px2-deploy: done; ZyperOS is restarting"
	fi
}

case $1 in
--status)
	$SSH "if [ -d $SAVE ]; then
		echo \"px2-deploy: kept since \$(cat $SAVE/since 2> /dev/null || echo ?)\"
		[ -d $SAVE/orig ] && (cd $SAVE/orig && find . -type f | sed 's|^\\./|  replaced |')
		[ -f $SAVE/added ] && sed 's|^|  added |' $SAVE/added
		true
	else echo 'px2-deploy: nothing kept: the card is as no deploy left it'; fi" < /dev/null
	exit 0 ;;
--restore)
	# Each kept file back, each added one gone, then the folder: one round trip
	DONE=$($SSH "set -e
		[ -d $SAVE ] || { echo 'px2-deploy: nothing kept, nothing to restore' >&2; exit 3; }
		if [ -d $SAVE/orig ]; then
			cd $SAVE/orig
			find . -type f | while read -r f; do
				rel=\${f#./}
				mkdir -p \"$DEST/\$(dirname \"\$rel\")\"
				mv -f \"\$f\" \"$DEST/\$rel\"
				echo \"\$rel\"
			done
		fi
		if [ -f $SAVE/added ]; then
			while read -r rel; do rm -f \"$DEST/\$rel\"; echo \"\$rel\"; done < $SAVE/added
		fi
		cd / && rm -rf $SAVE && sync" < /dev/null) || exit $?
	printf '%s\n' "$DONE" | sed '/^$/d; s/^/  restored /'
	restart "$DONE"
	exit 0 ;;
-*) usage ;;
esac

SRC=$1
[ -f "$SRC/zyperos.elf" ] || { echo "px2-deploy: no zyperos.elf in $SRC" >&2; exit 1; }
KEEP=${PX2_DEPLOY_KEEP:-1}

# What the card has, by checksum, in one round trip
REMOTE=$($SSH "cd $DEST && find . -type f -exec sha256sum {} +" < /dev/null || true)

cd "$SRC"
LIST=$(mktemp)
trap 'rm -f "$LIST"' EXIT
find . -type f ! -name '._*' ! -name .DS_Store | sort | while read -r f; do
	want=$(shasum -a 256 "$f" | cut -d' ' -f1)
	# The name is everything after the checksum's two spaces: a file whose
	# name has a space in it (a menu music track's) matched by $2 never would
	have=$(printf '%s\n' "$REMOTE" | awk -v f="$f" '{ h = $1; sub(/^[^ ]+  /, ""); if ($0 == f) print h }')
	[ "$want" = "$have" ] && continue
	rel=${f#./}
	$SSH "mkdir -p $DTMP" < /dev/null
	$SCP "$f" "root@$HOST:$DTMP/file" < /dev/null
	# The card's own file kept before it is replaced, the first time only;
	# a file the card did not have, named as added
	$SSH "set -e; got=\$(sha256sum $DTMP/file | cut -d' ' -f1)
		[ \"\$got\" = $want ] || { echo 'px2-deploy: $rel arrived damaged' >&2; exit 1; }
		if [ $KEEP = 1 ] && [ ! -e '$SAVE/orig/$rel' ] && ! grep -qxF '$rel' '$SAVE/added' 2> /dev/null; then
			mkdir -p '$SAVE'
			[ -f '$SAVE/since' ] || date '+%Y-%m-%d %H:%M:%S' > '$SAVE/since'
			if [ -e '$DEST/$rel' ]; then
				mkdir -p \"$SAVE/orig/\$(dirname '$rel')\"
				cp -p '$DEST/$rel' '$SAVE/orig/$rel'
			else
				echo '$rel' >> '$SAVE/added'
			fi
		fi
		mkdir -p \"$DEST/\$(dirname '$rel')\"
		mv -f $DTMP/file '$DEST/$rel'
		case '$rel' in *.sh|zyperos.elf|diatom|muse|btplayer|p2d|portkeys|zyhome|zyperos-update|openbor/OpenBOR) chmod +x '$DEST/$rel' ;; esac" < /dev/null
	echo "  sent $rel"
	echo "$rel" >> "$LIST"
done
$SSH "sync; rm -rf $DTMP" < /dev/null
[ "$KEEP" = 1 ] && [ -s "$LIST" ] && echo "px2-deploy: the card's own files are kept: tools/px2-deploy.sh --restore puts them back"
restart "$(cat "$LIST")"
