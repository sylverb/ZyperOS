#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# QEMU's image as a release builds it, from this commit, to try before a
# release puts one out, or to try tools/qemu-run.sh fetch on (docs/qemu.md,
# "Setting it up"):
#
#   tools/qemu-image.sh
#
# plastron's make release-qemu from a clean worktree of HEAD, as
# tools/release.sh runs it: an empty output folder, hours, with
# /tmp/plastron-build-lock held all the while. Into out/qemu-image/:
# ZyperOS-v<VERSION>-qemu.tar.xz and SHA256SUMS, a release's files as fetch
# reads them, so
#
#   ZYPEROS_QEMU_RELEASE_URL=file://$PWD/out/qemu-image tools/qemu-run.sh fetch
#
# fetches it as it would the release's. Nothing leaves this computer: a
# release's image is tools/release.sh's, on the release with the devices'.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
# The cores: vendor/ of the main checkout, as tools/release.sh takes them
VENDOR=${ZYPEROS_VENDOR:-$(git worktree list --porcelain | sed -n '1s/^worktree //p')/vendor}
VERSION=$(sed -n 's/^VERSION ?= //p' Makefile)
NAME=ZyperOS-v$VERSION-qemu.tar.xz
OUT=$ROOT/out/qemu-image
LOCK=/tmp/plastron-build-lock

stop() { echo "qemu-image: $*" >&2; exit 1; }
[ $# -eq 0 ] || { sed -n '3,17p' "$0" | sed 's/^# \{0,1\}//' >&2; exit 2; }
[ -z "$(git status --porcelain)" ] || stop "the working tree has changes: commit them first"
[ -d "$VENDOR/cores" ] || stop "no cores in $VENDOR/cores (ZYPEROS_VENDOR)"
sh mk/core-stamp.sh check "$VENDOR" || stop "a patched core in $VENDOR is stale or unstamped"
HEAD=$(git rev-parse HEAD)
echo "QEMU's image of ZyperOS $VERSION at $(git rev-parse --short HEAD), plastron/ with it"

# The source as committed, never the working tree
SRC=$(mktemp -d)/zyperos
HAVE_LOCK=0
cleanup() {
	git worktree remove --force "$SRC" 2>/dev/null || true
	if [ $HAVE_LOCK = 1 ]; then rm -rf $LOCK; echo "qemu-image: $LOCK released"; fi
}
trap cleanup EXIT
git worktree add -q --detach "$SRC" "$HEAD"

until mkdir $LOCK 2>/dev/null; do
	echo "qemu-image: $LOCK held by $(cat $LOCK/owner 2>/dev/null || echo '?'); trying again in 30 s"
	sleep 30
done
HAVE_LOCK=1
echo "ZyperOS QEMU image of $(git rev-parse --short HEAD) (tools/qemu-image.sh)" > $LOCK/owner
date
make -C "$SRC/plastron" release-qemu VERSION="$VERSION" ZYPEROS_SRC="$SRC" ZYPEROS_VENDOR="$VENDOR"
rm -rf "$OUT"
mkdir -p "$OUT"
cp "$SRC/plastron/out/release-qemu/$NAME" "$OUT/"
rm -rf $LOCK
HAVE_LOCK=0
echo "qemu-image: $LOCK released"
date

cd "$OUT"
xz -t "$NAME"
names=$(tar -tf "$NAME" | LC_ALL=C sort | tr '\n' ' ')
[ "$names" = "Image sdcard.img u-boot.bin " ] || stop "$NAME holds $names"
shasum -a 256 "$NAME" > SHA256SUMS
printf 'ZyperOS %s (%s), plastron/ included\n' "$VERSION" "$HEAD" > BUILT-FROM
cat SHA256SUMS
echo "Built in $OUT. Fetch it as a release's: ZYPEROS_QEMU_RELEASE_URL=file://$OUT tools/qemu-run.sh fetch"
