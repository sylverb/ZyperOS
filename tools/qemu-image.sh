#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# The QEMU image for a developer without plastron's build (docs/qemu.md,
# "Setting it up"): plastron's QEMU image, built from a commit as a
# release is and published on GitHub, so ZyperOS runs in QEMU and in the
# Simulator on a Mac with no Docker build of its own. tools/qemu/image.pin
# names it, and tools/qemu-run.sh fetch downloads it by that pin.
#
#   tools/qemu-image.sh            build it from this commit and check it:
#                                  plastron's make release-qemu, from a clean
#                                  worktree of HEAD. Nothing leaves this
#                                  computer: out/qemu-image/, with the image
#                                  unpacked in out/qemu-image/image/ to try
#                                  first (tools/qemu-run.sh start --image)
#   tools/qemu-image.sh --publish [--rate RATE]
#                                  publish what that built, as the files
#                                  tried are: a GitHub pre-release
#                                  qemu-<the build's date> ($QEMU_TAG for
#                                  another) with the image, its sources and
#                                  SHA256SUMS, each upload checked against
#                                  GitHub's own sha256 of it before the
#                                  release is published. Then it writes
#                                  tools/qemu/image.pin, to commit. --rate
#                                  holds each upload under a slow uplink's
#                                  speed (curl's --limit-rate: 170k, say). A
#                                  --publish cut short goes on from the
#                                  draft it left.
#
# What it carries: ZyperOS as at the commit it was built from, on the Pixel
# 2's system for QEMU's virt machine, and no SSH key (plastron's
# board/qemu/post-build.sh: the key comes from QEMU at every boot; its
# scripts/release-qemu.sh checks the root filesystem for keys of any kind
# before it packs it). A later ZyperOS goes onto a VM the usual way
# (tools/qemu-run.sh deploy); a new image is for a change to plastron's
# system that the VM needs.
#
# Not a ZyperOS version: its tag is qemu-<date>, never v<number>, and it is a
# pre-release, never "latest", so System Update, the installer and the site
# never take it for one (as with the cores' release, mk/release-cores.sh). A
# release's files never change: a new image is a new tag, and a new pin.
#
# The build starts from an empty output folder, as a release's does, so it
# takes hours, and holds /tmp/plastron-build-lock all the time it runs, as
# tools/release.sh does.
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
REPO=tarikbc/ZyperOS
# The cores: vendor/ of the main checkout, as tools/release.sh takes them
VENDOR=${ZYPEROS_VENDOR:-$(git worktree list --porcelain | sed -n '1s/^worktree //p')/vendor}
OUT=$ROOT/out/qemu-image
LOCK=/tmp/plastron-build-lock
PIN=tools/qemu/image.pin
ARCHIVE=zyperos-qemu.tar.xz
SOURCES=zyperos-qemu-sources.tar
# What the archive holds, as tools/qemu-run.sh fetch takes it
FILES="Image sdcard.img u-boot.bin"

PUBLISH=0
RATE=
usage() { echo "usage: tools/qemu-image.sh [--publish [--rate RATE]]" >&2; exit 2; }
while [ $# -gt 0 ]; do
	case $1 in
	--publish) PUBLISH=1 ;;
	--rate) [ $# -ge 2 ] && [ -n "$2" ] || usage; RATE=$2; shift ;;
	*) usage ;;
	esac
	shift
done
[ $PUBLISH = 1 ] || [ -z "$RATE" ] || usage

say() { printf '\n== %s\n' "$*"; }
stop() { echo "qemu-image: $*" >&2; exit 1; }

# The archive is exactly the image's files
check_archive() {
	xz -t "$1" || stop "$1 is not a whole xz file"
	names=$(tar -tf "$1" | LC_ALL=C sort | tr '\n' ' ')
	[ "$names" = "$FILES " ] || stop "$1 holds $names, not $FILES"
}

if [ $PUBLISH = 0 ]; then
	say "Checks"
	[ -z "$(git status --porcelain)" ] || stop "the working tree has changes: commit them first"
	git fetch -q origin
	HEAD=$(git rev-parse HEAD)
	# The release names the commit it was built from, so GitHub must have it
	git merge-base --is-ancestor "$HEAD" origin/main ||
		stop "$(git rev-parse --short HEAD) is not on main as GitHub has it: build from main"
	[ -d "$VENDOR/cores" ] || stop "no cores in $VENDOR/cores (ZYPEROS_VENDOR)"
	sh mk/core-stamp.sh check "$VENDOR" || stop "a patched core in $VENDOR is stale or unstamped"
	echo "QEMU's image of ZyperOS at $(git rev-parse --short HEAD), plastron/ with it"

	# The source as committed, never the working tree
	SRC=$(mktemp -d)/zyperos
	HAVE_LOCK=0
	cleanup() {
		git worktree remove --force "$SRC" 2>/dev/null || true
		if [ $HAVE_LOCK = 1 ]; then rm -rf $LOCK; echo "qemu-image: $LOCK released"; fi
	}
	trap cleanup EXIT
	git worktree add -q --detach "$SRC" "$HEAD"

	say "Waiting for $LOCK"
	until mkdir $LOCK 2>/dev/null; do
		echo "  held by $(cat $LOCK/owner 2>/dev/null || echo '?'); trying again in 30 s"
		sleep 30
	done
	HAVE_LOCK=1
	echo "ZyperOS QEMU image of $(git rev-parse --short HEAD) (tools/qemu-image.sh)" > $LOCK/owner

	say "plastron's release build of QEMU's image (everything, from empty; this takes a while)"
	date
	make -C "$SRC/plastron" release-qemu ZYPEROS_SRC="$SRC" ZYPEROS_VENDOR="$VENDOR"
	rm -rf "$OUT"
	mkdir -p "$OUT"
	cp "$SRC/plastron/out/release-qemu/$ARCHIVE" "$SRC/plastron/out/release-qemu/$SOURCES" "$OUT/"
	rm -rf $LOCK
	HAVE_LOCK=0
	echo "qemu-image: $LOCK released"
	date

	say "Checking it"
	cd "$OUT"
	check_archive "$ARCHIVE"
	tar -tf "$SOURCES" > /dev/null || stop "$SOURCES is not a whole tar file"
	shasum -a 256 "$ARCHIVE" "$SOURCES" > SHA256SUMS
	printf 'ZyperOS %s (%s), plastron/ included, %s\n' "$(sed -n 's/^VERSION ?= //p' "$ROOT/Makefile")" \
		"$HEAD" "$(date -u +%Y-%m-%d)" > BUILT-FROM
	mkdir image
	tar -xf "$ARCHIVE" -C image
	cat SHA256SUMS
	ls -l
	echo
	echo "Built in $OUT. Try it first, as published it would be:"
	echo "  tools/qemu-run.sh start -n try --fresh --image $OUT/image"
	echo "Nothing was published: tools/qemu-image.sh --publish does that."
	exit 0
fi

say "Checks"
[ -f "$OUT/BUILT-FROM" ] || stop "nothing built in $OUT: run tools/qemu-image.sh first"
BUILT_HEAD=$(sed -n 's/^ZyperOS .* (\([0-9a-f]*\)), plastron.*/\1/p' "$OUT/BUILT-FROM")
BUILT_DATE=$(sed -n 's/.*, plastron\/ included, //p' "$OUT/BUILT-FROM")
[ -n "$BUILT_HEAD" ] && [ -n "$BUILT_DATE" ] || stop "$OUT/BUILT-FROM names no commit and date: $(cat "$OUT/BUILT-FROM")"
TAG=${QEMU_TAG:-qemu-$BUILT_DATE}
printf '%s\n' "$TAG" | grep -qx 'qemu-[0-9A-Za-z.-]*' || stop "$TAG is not a qemu-<date> tag"
(cd "$OUT" && shasum -a 256 -c SHA256SUMS > /dev/null) || stop "a file in $OUT no longer matches its SHA256SUMS"
check_archive "$OUT/$ARCHIVE"
git fetch -q origin
git merge-base --is-ancestor "$BUILT_HEAD" origin/main 2>/dev/null ||
	stop "$BUILT_HEAD, which $OUT was built from, is not on main as GitHub has it"
command -v gh > /dev/null && gh auth status > /dev/null 2>&1 || stop "gh isn't signed in"
SUM=$(awk -v f="$ARCHIVE" '$2 == f { print $1 }' "$OUT/SHA256SUMS")
echo "$TAG: ZyperOS at $(printf '%.7s' "$BUILT_HEAD"), built $BUILT_DATE, $ARCHIVE $SUM"

# The release as a draft until every file is up and checked; a draft from a
# --publish cut short is the one to go on with
found=$(gh api "repos/$REPO/releases?per_page=50" --jq ".[] | select(.tag_name == \"$TAG\") | \"\(.id) \(.draft)\"")
if [ -n "$found" ]; then
	[ "${found#* }" = true ] || stop "$TAG is published already: a new image is a new tag (QEMU_TAG=$TAG.2, say)"
	ID=${found% *}
	echo "qemu-image: going on with the draft $TAG left"
else
	CORES_TAG=$(sed -n 's/^CORES_TAG=//p' mk/fetch-vendor.sh)
	cat > "$OUT/NOTES.md" <<-EOF
	plastron's QEMU image of ZyperOS: the GKD Pixel 2's system built for QEMU's arm64 \`virt\` machine, with ZyperOS as at $BUILT_HEAD, for running ZyperOS and the ZyperOS Simulator on a Mac without building plastron. \`tools/qemu-run.sh fetch\` downloads it into \`~/Library/Caches/zyperos-qemu/image\` and checks it against the sha256 in \`tools/qemu/image.pin\` ([docs/qemu.md](https://github.com/$REPO/blob/main/docs/qemu.md)).

	This is not a ZyperOS version and does not go on a handheld: ZyperOS's own releases do.

	It has no SSH key in it. \`tools/qemu-run.sh\` hands the VM the key of whoever starts it, at every boot.

	## Files

	- \`$ARCHIVE\`: \`sdcard.img\`, \`Image\` and \`u-boot.bin\`, what \`tools/qemu-run.sh\` boots.
	- \`$SOURCES\`: the licenses and sources of everything in the image (Buildroot's legal-info, with U-Boot's source and Buildroot itself). The libretro cores' sources are in the cores release, [$CORES_TAG](https://github.com/$REPO/releases/tag/$CORES_TAG).
	- \`SHA256SUMS\`: both files' sha256.
	EOF
	say "The draft $TAG"
	ID=$(gh api "repos/$REPO/releases" -f tag_name="$TAG" -f target_commitish="$BUILT_HEAD" \
		-f name="QEMU image $TAG" -F body=@"$OUT/NOTES.md" -F draft=true -F prerelease=true \
		-f make_latest=false --jq .id)
fi

# Each file streamed from disk (a 1.5 GB one too), and checked against
# GitHub's own sha256 of what it got. One left half uploaded, or another
# file of the same name, is deleted first
AUTH=$(mktemp)
chmod 600 "$AUTH"
trap 'rm -f "$AUTH"' EXIT
printf 'Authorization: Bearer %s\n' "$(gh auth token)" > "$AUTH"
asset() { # asset NAME: "ID STATE DIGEST" of the draft's file of that name
	gh api "repos/$REPO/releases/$ID/assets?per_page=100" \
		--jq ".[] | select(.name == \"$1\") | \"\(.id) \(.state) \(.digest)\""
}
for f in SHA256SUMS "$ARCHIVE" "$SOURCES"; do
	want=sha256:$(shasum -a 256 "$OUT/$f" | cut -c1-64)
	have=$(asset "$f")
	if [ -n "$have" ] && [ "${have#* }" = "uploaded $want" ]; then
		echo "  up already  $f"
		continue
	fi
	[ -z "$have" ] || gh api -X DELETE "repos/$REPO/releases/assets/${have%% *}" > /dev/null
	echo "  uploading   $f ($(du -h "$OUT/$f" | cut -f1))"
	curl -sS --fail-with-body -X POST -T "$OUT/$f" ${RATE:+--limit-rate "$RATE"} -H @"$AUTH" \
		-H 'Content-Type: application/octet-stream' -o /dev/null \
		"https://uploads.github.com/repos/$REPO/releases/$ID/assets?name=$f" ||
		stop "the upload of $f stopped: run --publish again to go on from the draft"
	have=$(asset "$f")
	[ "${have#* }" = "uploaded $want" ] || stop "GitHub has $f as '${have#* }', not 'uploaded $want'"
done
rm -f "$AUTH"

# Published, as a pre-release that is never the latest. A draft's PATCH sends
# its tag_name, or GitHub takes the tag away
say "Publishing $TAG"
gh api -X PATCH "repos/$REPO/releases/$ID" -f tag_name="$TAG" -F draft=false -F prerelease=true \
	-f make_latest=false > /dev/null
state=$(gh release view "$TAG" --repo $REPO --json tagName,isDraft,isPrerelease --jq '"\(.tagName) \(.isDraft) \(.isPrerelease)"')
[ "$state" = "$TAG false true" ] || stop "GitHub has the release as '$state', not '$TAG false true'"

# The pin, for fetch: committed, it is what every checkout downloads
sed -i '' -e "s/^tag=.*/tag=$TAG/" -e "s/^sha256=.*/sha256=$SUM/" "$ROOT/$PIN"
echo
echo "Published: https://github.com/$REPO/releases/tag/$TAG"
echo "$PIN names it now. Commit it (a pull request) for tools/qemu-run.sh fetch to take it:"
git -C "$ROOT" diff -- "$PIN"
