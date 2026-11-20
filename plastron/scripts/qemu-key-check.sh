#!/bin/sh
# Whether QEMU's image lets nobody in by itself, for scripts/release-qemu.sh,
# before it packs the image to publish:
#
#   sh qemu-key-check.sh TARGET_DIR ROOTFS_SQUASHFS UNSQUASHFS
#
# What gets the VM in is there: dropbear at boot, and S49sshkey, which takes
# root's key from QEMU (board/qemu/post-build.sh). And no key is in what
# ships, checked on the root filesystem as squashed, not trusted: root's .ssh
# only the link to /run/ssh-root, dropbear's host keys only the link to
# /var/run (made at boot), no file named as a key is, and no text file in the
# target holds a private key, whatever its name. The boot partition holds the
# slots' kernels and boot.scr alone (board/qemu/genimage.cfg).
set -eu
TARGET=$1
SQ=$2
UNSQ=$3
fail() { echo "qemu-key-check: $*" >&2; exit 1; }

for f in etc/init.d/S50dropbear etc/init.d/S49sshkey usr/sbin/dropbear; do
	[ -x "$TARGET/$f" ] || fail "/$f is not in the image; ZyperOS's tools/qemu-run.sh needs SSH"
done

LIST=$(mktemp)
trap 'rm -f "$LIST"' EXIT
# Each line ends in squashfs-root/PATH, or squashfs-root/PATH -> TARGET
"$UNSQ" -lls "$SQ" > "$LIST" || fail "could not list $SQ"
grep -q ' squashfs-root/root/\.ssh -> /run/ssh-root$' "$LIST" ||
	fail "/root/.ssh in $SQ is not the link to /run/ssh-root"
grep -q ' squashfs-root/etc/dropbear -> /var/run/dropbear$' "$LIST" ||
	fail "/etc/dropbear in $SQ is not the link to /var/run/dropbear"
if grep -E '/(authorized_keys2?|[^/ ]*_host_key|id_(rsa|dsa|ecdsa|ed25519)(_sk)?)( |$)' "$LIST"; then
	fail "$SQ carries the key files above"
fi
# A program that knows the format (libcrypto) is binary and passes: grep -I
if grep -rlIE -- '-----BEGIN ([A-Z0-9]+ )*PRIVATE KEY-----' "$TARGET"; then
	fail "the files above hold a private key"
fi
echo "qemu-key-check: no SSH key and no private key in the image"
