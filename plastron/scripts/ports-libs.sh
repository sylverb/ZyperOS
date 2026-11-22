#!/bin/sh
# Whether a built root file system has every library PortMaster's ports and
# their runtimes load from the system (buildroot/board/zyperos/ports-libs.txt),
# for scripts/release.sh, before it packs the Pixel 2's image:
#
#   sh ports-libs.sh TARGET_DIR
#
# Each soname must be in TARGET_DIR's /usr/lib or /lib, as a file or a link
# that leads to one there (Buildroot's links are relative, so they resolve
# inside TARGET_DIR); a link to nothing is missing. Says each one missing,
# what it is for, and exits 1 if any is. A port missing one stops at its
# start with "error while loading shared libraries", as every Westonpack port
# did for libevdev.so.2 (1.1.0-rc.6). ZyperOS's make check-portlibs runs this
# over roots of its own making.
set -u
TARGET=${1:?usage: ports-libs.sh TARGET_DIR}
LIST=${PORTS_LIBS:-$(dirname "$0")/../buildroot/board/zyperos/ports-libs.txt}
[ -d "$TARGET" ] || { echo "ports-libs: no folder $TARGET" >&2; exit 2; }
[ -f "$LIST" ] || { echo "ports-libs: no list $LIST" >&2; exit 2; }

missing=0 n=0
while read -r so pkg opt who; do
	case $so in '' | '#'*) continue ;; esac
	n=$((n + 1))
	if [ ! -e "$TARGET/usr/lib/$so" ] && [ ! -e "$TARGET/lib/$so" ]; then
		echo "ports-libs: $so is not in the image ($pkg, $opt): $who" >&2
		missing=$((missing + 1))
	fi
done < "$LIST"
if [ "$n" -eq 0 ]; then
	echo "ports-libs: $LIST lists no library" >&2
	exit 2
fi
[ "$missing" -eq 0 ] || exit 1
echo "ports-libs: all $n libraries the ports load are in the image"
