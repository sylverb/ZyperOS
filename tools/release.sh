#!/bin/bash
# A ZyperOS release, from this Mac.
#
#   tools/release.sh            build the card image of every device in
#                               config/devices from this commit and check
#                               them; nothing leaves this computer
#                               (out/release/v<VERSION>/)
#   tools/release.sh --publish  the same, then a draft GitHub release
#                               v<VERSION> with each device's image, its
#                               sources, its System Update packs and signed
#                               lists, and SHA256SUMS, and the tag pushed
#   tools/release.sh --publish-built
#                               publish what the first form built, without
#                               building again: the files tested on a device
#                               are the files released. Refused unless they
#                               were built from main as GitHub has it now (or
#                               from a commit before it that only lacked the
#                               notes' Tested on lines) and still match their
#                               SHA256SUMS
#
# Both --publish forms refuse a release whose notes lack a "Tested on <id>:"
# line for any device in config/devices (tools/release-devices.sh tested,
# docs/releases/README.md): every device a release carries has passed its
# device test for that version first.
#
#   tools/release.sh --kit IDS [--publish | --publish-built]
#                               a test kit instead: the same, for the devices
#                               IDS only ("brick", or "brick pixel2"), at a
#                               pre-release version like 1.0.6-kit.2. GitHub
#                               never makes a pre-release the latest, so
#                               System Update, zyperos.com and every
#                               installer but the kit's own pass it by: the
#                               kit's installer offers its own kit
#                               (installer/src-tauri/src/release.rs). A kit is
#                               the device test, so its notes need no Tested
#                               on lines; it is made from a commit whose only
#                               change from main is the version (tools/
#                               set-version.sh) and its notes, and it carries
#                               no System Update files, as System Update
#                               only reads the latest. Nothing goes to
#                               Discord's #releases: a kit's testers get it
#                               in its own thread.
#
# Both --publish forms end by waiting for GitHub to publish the release, then
# post its notes in the Discord's #releases channel (tools/discord-release.py,
# with Zyper's token from ~/.config/zyperos/discord-bot.token). Ctrl-C stops
# the wait; tools/discord-release.py v<VERSION> posts them later.
#
# (make release, and make release PUBLISH=1.) The tag starts GitHub's
# .github/workflows/release.yml, which builds ZyperOS Installer for macOS,
# Windows and Linux, adds the apps to the same release, puts them in its
# SHA256SUMS, and publishes it: every version has its image and its
# installer in one place.
#
# The version is the Makefile's VERSION (tools/set-version.sh sets it with the
# installer's). The release notes are docs/releases/v<VERSION>.md, committed.
#
# The image is built here, not on GitHub, because its inputs are here: the
# cores in the main checkout's vendor/ (ZYPEROS_VENDOR), which
# mk/fetch-vendor.sh can no longer fetch exactly as they are, and plastron's
# Docker volume. plastron's release build of a device (make release-<id>)
# starts from an empty output folder of its own, so it builds everything
# again, with the shared download cache: it takes a while, and this holds
# /tmp/plastron-build-lock all the time it runs.
#
# The devices are config/devices: a device joins it with its first release
# and stays, as System Update and the installer read releases/latest. The
# TrimUI Brick is not in it yet; its test kits carry it (--kit brick). Its
# release-brick builds its kernel and modules first (plastron's make
# brick-kernel), and needs the builder's own copy of its closed files
# (plastron's make brick-fetch, into ~/brick-runtime), which build it and are
# never released: each player's installer gets them (installer/README.md).
set -euo pipefail
cd "$(dirname "$0")/.."
ROOT=$PWD
REPO=tarikbc/ZyperOS
# The cores: vendor/ of the main checkout (git's first worktree), which a
# worktree like this one may not be
VENDOR=${ZYPEROS_VENDOR:-$(git worktree list --porcelain | sed -n '1s/^worktree //p')/vendor}
VERSION=$(sed -n 's/^VERSION ?= //p' Makefile)
TAG=v$VERSION
OUT=$ROOT/out/release/$TAG
NOTES=docs/releases/$TAG.md
LOCK=/tmp/plastron-build-lock
# Every released device, in config/devices' order
DEVICES=$(sh tools/release-devices.sh list) || exit 1
DEVICES=$(echo $DEVICES)
# A device's files, as plastron's scripts/release.sh names them
image() { echo "ZyperOS-$TAG-$1.img.xz"; }
sources() { echo "ZyperOS-$TAG-$1-sources.tar"; }
# System Update (docs/updates.md): plastron packs the image's own ZyperOS
# folder and writes the list unsigned; it is signed here, on the Mac that
# holds the key (tools/update-key.sh), never in the container or on GitHub.
# The list's name has no version, so releases/latest/download/ finds it.
# Then the system's (root, kernel, device tree) for a card with two system
# slots (docs/updates.md, layer 2), signed the same way: pack, list, pack,
# list
update_files() { echo "ZyperOS-$TAG-$1-update.zpk ZyperOS-$1-update.txt ZyperOS-$TAG-$1-system.zpk ZyperOS-$1-system.txt"; }
# Every device's image and sources, for the release
RELEASE_FILES=
for d in $DEVICES; do RELEASE_FILES="$RELEASE_FILES $(image $d) $(sources $d)"; done
UPDATE_KEY=${ZYPEROS_UPDATE_KEY:-$HOME/.config/zyperos/update-signing-key.pem}

PUBLISH=0
BUILT=0
KIT=
usage() { echo "usage: tools/release.sh [--kit IDS] [--publish | --publish-built]" >&2; exit 2; }
while [ $# -gt 0 ]; do
	case $1 in
	--publish) PUBLISH=1 ;;
	--publish-built) PUBLISH=1; BUILT=1 ;;
	--kit) [ $# -ge 2 ] && [ -n "$2" ] || usage; KIT=$2; shift ;;
	*) usage ;;
	esac
	shift
done
if [ -n "$KIT" ]; then
	# The kit's devices, checked as config/devices is: each one id, once
	KIT_LIST=$(mktemp)
	echo "$KIT" | tr ' ' '\n' > "$KIT_LIST"
	DEVICES=$(sh tools/release-devices.sh list "$KIT_LIST") || { rm -f "$KIT_LIST"; exit 1; }
	rm -f "$KIT_LIST"
	DEVICES=$(echo $DEVICES)
	for d in $DEVICES; do
		[ -f "src/device/${d}_profile.c" ] || { echo "release.sh: no device $d (src/device/${d}_profile.c)" >&2; exit 1; }
	done
	RELEASE_FILES=
	for d in $DEVICES; do RELEASE_FILES="$RELEASE_FILES $(image $d) $(sources $d)"; done
fi

say() { printf '\n== %s\n' "$*"; }
stop() { echo "release.sh: $*" >&2; exit 1; }

# The draft and the tag, from the files in $OUT
publish() {
	say "Publishing $TAG"
	# A draft first, with the image on it: the tag's workflow adds the installer
	# to this release and publishes it, and refuses a tag with no draft waiting
	[ $UPDATES = 0 ] || [ -n "$UPDATE_FILES" ] || stop "the update list is not signed"
	# A kit's commit is on no branch GitHub has: the draft names main, and
	# the tag, pushed before the workflow publishes it, is what it is made from
	TARGET=$HEAD
	[ -z "$KIT" ] || TARGET=main
	gh release create "$TAG" --repo $REPO --draft $PRERELEASE --target "$TARGET" --title "ZyperOS $VERSION" \
		--notes-file "$ROOT/$NOTES" $RELEASE_FILES $UPDATE_FILES SHA256SUMS
	cd "$ROOT"
	git tag -a "$TAG" -m "ZyperOS $VERSION" "$HEAD"
	git -c credential.helper= -c "credential.helper=!gh auth git-credential" push origin "refs/tags/$TAG"
	echo
	echo "The draft is up and $TAG is pushed. GitHub now builds ZyperOS Installer and publishes the release:"
	echo "  https://github.com/$REPO/actions/workflows/release.yml"
	if [ -n "$KIT" ]; then
		echo "A test kit: nothing goes to #releases. Its page, for the kit's thread: https://github.com/$REPO/releases/tag/$TAG"
		return 0
	fi
	say "Discord"
	if [ -f "$HOME/.config/zyperos/discord-bot.token" ]; then
		echo "Waiting for GitHub to publish $TAG; then its notes go to #releases (Ctrl-C stops the wait)"
		python3 tools/discord-release.py --wait "$TAG" ||
			echo "release.sh: the notes did not go to Discord; run tools/discord-release.py $TAG once $TAG is published" >&2
	else
		echo "No Discord bot token here: run tools/discord-release.py $TAG once $TAG is published"
	fi
}

say "Checks"
sh tools/check-version.sh
# A version with a hyphen (0.2.0-beta.1) is a pre-release, which the
# installer never offers
case $VERSION in *-*) PRERELEASE=--prerelease ;; *) PRERELEASE= ;; esac
[ -z "$KIT" ] || [ -n "$PRERELEASE" ] ||
	stop "a kit's version is a pre-release, like 1.0.6-kit.2, not $VERSION: tools/set-version.sh sets it, on a branch of its own"
[ -z "$(git status --porcelain)" ] || stop "the working tree has changes: commit them first"
git fetch -q origin
HEAD=$(git rev-parse HEAD)
# A kit: main as GitHub has it at some commit, with only its version and its
# notes changed. Each version file's change is its version line alone
# version_blank FILE REV: FILE at REV with ZyperOS's version line blanked, as
# tools/set-version.sh finds it: the Makefile's VERSION, the installer's
# package version, and its entry's in Cargo.lock (not a dependency's)
version_blank() {
	git show "$2:$1" | case $1 in
	Makefile) sed 's/^VERSION ?= .*/VERSION ?= -/' ;;
	installer/src-tauri/Cargo.toml) awk '!done && /^version = "/ { $0 = "version = -"; done = 1 } { print }' ;;
	*) awk 'prev == "name = \"zyperos-installer\"" && /^version = "/ { $0 = "version = -" } { print; prev = $0 }' ;;
	esac
}
kit_change() {
	BASE=$(git merge-base HEAD origin/main) || stop "$(git rev-parse --short HEAD) shares no commit with origin/main"
	CHANGED=$(git diff --name-only "$BASE" HEAD) || stop "git can't compare $(git rev-parse --short HEAD) with main"
	for f in $CHANGED; do
		case $f in
		Makefile | installer/src-tauri/Cargo.toml | installer/src-tauri/Cargo.lock)
			[ "$(version_blank "$f" "$BASE")" = "$(version_blank "$f" HEAD)" ] ||
				stop "a kit changes only the version line of $f; this commit changes more of it" ;;
		"$NOTES") ;;
		*) stop "a kit changes only the version and $NOTES from main; this commit changes $f too (main at $(git rev-parse --short "$BASE"))" ;;
		esac
	done
	echo "release.sh: a kit of main at $(git rev-parse --short "$BASE"), with its version and notes"
}
if [ $PUBLISH = 1 ]; then
	if [ -n "$KIT" ]; then
		kit_change
	else
		[ "$HEAD" = "$(git rev-parse origin/main)" ] ||
			stop "a release is made from main as GitHub has it: this is $(git rev-parse --short HEAD), origin/main is $(git rev-parse --short origin/main)"
	fi
	[ -f "$NOTES" ] || stop "write the release notes in $NOTES and commit them first"
	# Each device passed its device test for this version: its Tested on line.
	# A kit is that test
	[ -n "$KIT" ] || sh tools/release-devices.sh tested "$NOTES" ||
		stop "$NOTES needs a \"Tested on <id>: <who, on what>\" line for each of $DEVICES (docs/releases/README.md)"
	gh auth status >/dev/null 2>&1 || stop "gh isn't signed in"
	! git ls-remote --exit-code --tags origin "refs/tags/$TAG" >/dev/null 2>&1 || stop "$TAG is already tagged on GitHub"
	! gh release view "$TAG" --repo $REPO >/dev/null 2>&1 || stop "$REPO already has a $TAG release (a draft counts)"
fi
[ -d "$VENDOR/cores" ] || stop "no cores in $VENDOR/cores (ZYPEROS_VENDOR)"
# The patched cores against their stamps now, not an hour into the build,
# where mk/payload.sh asks the same (mk/core-stamp.sh)
sh mk/core-stamp.sh check "$VENDOR" || stop "a patched core in $VENDOR is stale or unstamped"
# With no public key in res/update/keys the image has System Update off, and
# the release has no update files. With one, a published release must carry a
# signed list: the private key is checked for now, before the long build.
UPDATES=0
ls res/update/keys/*.pem > /dev/null 2>&1 && UPDATES=1
# A kit carries none: System Update only reads the latest release
[ -z "$KIT" ] || UPDATES=0
if [ $UPDATES = 1 ]; then
	if [ -f "$UPDATE_KEY" ]; then
		KEY_ID=$(sh tools/update-key.sh id "$UPDATE_KEY")
		[ -f "res/update/keys/$KEY_ID.pem" ] ||
			stop "$UPDATE_KEY is key $KEY_ID, which res/update/keys does not have: no image would trust its list"
	elif [ $PUBLISH = 1 ]; then
		stop "no System Update signing key at $UPDATE_KEY (ZYPEROS_UPDATE_KEY): a release needs its list signed"
	else
		echo "release.sh: no signing key at $UPDATE_KEY; the update list stays unsigned (not publishable)"
	fi
elif [ -n "$KIT" ]; then
	echo "release.sh: a kit has no System Update files"
else
	echo "release.sh: no key in res/update/keys: System Update is off in this image, and the release has no update files"
fi
echo "ZyperOS $VERSION from $(git rev-parse --short HEAD), plastron/ with it, for $DEVICES${KIT:+ (a test kit)}"

# --publish-built: the files of an earlier build, as they are
if [ $BUILT = 1 ]; then
	[ -f "$OUT/BUILT-FROM" ] || stop "nothing built in $OUT: run tools/release.sh first"
	BUILT_HEAD=$(sed -n 's/^ZyperOS .* (\([0-9a-f]*\)), plastron.*/\1/p' "$OUT/BUILT-FROM")
	[ -n "$BUILT_HEAD" ] || stop "$OUT/BUILT-FROM names no commit: $(cat "$OUT/BUILT-FROM")"
	if [ -n "$KIT" ] && [ "$BUILT_HEAD" != "$HEAD" ]; then
		# A kit carries no System Update files, so its notes are in none of
		# its files, and plastron builds no image from the installer, the
		# workflows or the docs: what else changed since it was built (its
		# notes, a fix to the installer) leaves the image as it is
		for f in $(git diff --name-only "$BUILT_HEAD" HEAD 2>/dev/null || echo unreadable); do
			case $f in
			installer/* | .github/* | docs/* | tools/release.sh) ;;
			*) stop "$OUT is from $(cat "$OUT/BUILT-FROM"); since then $f changed, which goes into the image, so build again" ;;
			esac
		done
		echo "release.sh: built from $(git rev-parse --short "$BUILT_HEAD"); since then only the installer, the workflows, the docs or this script changed"
	elif [ "$BUILT_HEAD" != "$HEAD" ]; then
		# The device tests come after the build, so their Tested on lines
		# may land after it: main may be the built commit and one that adds
		# them, and nothing else. The image and the update lists are the
		# same then (the lists' What's New comes from the notes' points,
		# which a Tested on line is not)
		untested() { git show "$1:$NOTES" 2>/dev/null | grep -v -e '^Tested on ' -e '^## Tested$' -e '^[[:space:]]*$' || true; }
		git merge-base --is-ancestor "$BUILT_HEAD" "$HEAD" 2>/dev/null &&
			[ "$(git diff --name-only "$BUILT_HEAD" "$HEAD")" = "$NOTES" ] &&
			[ "$(untested "$BUILT_HEAD")" = "$(untested "$HEAD")" ] ||
			stop "$OUT is from another commit: $(cat "$OUT/BUILT-FROM"); since then more changed than $NOTES's Tested on lines, so build again"
		echo "release.sh: built from $(git rev-parse --short "$BUILT_HEAD"); main since has only $NOTES's Tested on lines"
	fi
	cd "$OUT"
	shasum -a 256 -c SHA256SUMS > /dev/null || stop "a file in $OUT no longer matches SHA256SUMS"
	UPDATE_FILES=
	for d in $DEVICES; do
		for f in $(image $d) $(sources $d); do
			awk -v f="$f" '$2 == f { found = 1 } END { exit !found }' SHA256SUMS || stop "$OUT's SHA256SUMS has no $f"
		done
		xz -t "$(image $d)"
		[ $UPDATES = 1 ] || continue
		for f in $(update_files $d); do
			[ -s "$f" ] || stop "$OUT has no signed $f"
			UPDATE_FILES="$UPDATE_FILES $f"
		done
	done
	publish
	exit 0
fi

# The source as committed, never the working tree: a clean worktree of HEAD
SRC=$(mktemp -d)/zyperos
HAVE_LOCK=0
cleanup() {
	git worktree remove --force "$SRC" 2>/dev/null || true
	if [ $HAVE_LOCK = 1 ]; then rm -rf $LOCK; echo "release.sh: $LOCK released"; fi
}
trap cleanup EXIT
git worktree add -q --detach "$SRC" "$HEAD"
# plastron/, the system, from the same commit
PLASTRON=$SRC/plastron

say "Waiting for $LOCK"
until mkdir $LOCK 2>/dev/null; do
	echo "  held by $(cat $LOCK/owner 2>/dev/null || echo '?'); trying again in 30 s"
	sleep 30
done
HAVE_LOCK=1
echo "ZyperOS release $TAG (tools/release.sh)" > $LOCK/owner

make -C "$PLASTRON" builder volume
docker run --rm -v plastron-work:/work --user root plastron-builder rm -rf /work/release
# What comes out of the container: each device's image and sources, and its
# packs with their lists unsigned
GET=
for d in $DEVICES; do
	if [ "$d" = brick ] || [ "$d" = smartpro ]; then
		# Its kernel and modules first, from this commit's patches, as
		# plastron's make release builds them (board/brick/post-image.sh
		# stops without them); the Smart Pro's is the Brick's with its own
		say "The $d's kernel and modules"
		make -C "$PLASTRON" "$d-kernel"
	fi
	say "The $d image: plastron's release build (everything, from empty; this takes a while)"
	date
	make -C "$PLASTRON" "release-$d" VERSION="$VERSION" ZYPEROS_SRC="$SRC" ZYPEROS_VENDOR="$VENDOR"
	GET="$GET $(image $d) $(sources $d)"
	if [ $UPDATES = 1 ]; then
		set -- $(update_files $d)
		GET="$GET $1 $2.unsigned $3 $4.unsigned"
	fi
done
rm -rf "$OUT"
mkdir -p "$OUT"
docker run --rm -v plastron-work:/work -v "$OUT":/out --user root plastron-builder \
	sh -c "cd /work/release && cp $GET /out/ && chown -R $(id -u):$(id -g) /out"
rm -rf $LOCK
HAVE_LOCK=0
echo "release.sh: $LOCK released"
date

say "Checking the images"
cd "$OUT"
UPDATE_FILES=
# A pack and its list: the list describes this pack and this version, and is
# signed here with the release key
sign_list() { # sign_list PACK LIST
	[ -s "$1" ] && [ -s "$2.unsigned" ] || stop "plastron's release didn't make $1 and $2.unsigned"
	PACK_LINE=$(sed -n 's/^pack //p' "$2.unsigned")
	[ "$PACK_LINE" = "$1 $(wc -c < "$1" | tr -d ' ') $(shasum -a 256 "$1" | cut -c1-64)" ] ||
		stop "$2.unsigned does not describe $1"
	grep -qx "version $VERSION" "$2.unsigned" || stop "$2.unsigned is not for $VERSION"
	if [ -f "$UPDATE_KEY" ]; then
		sh "$ROOT/tools/update-key.sh" sign "$2.unsigned" "$2" "$UPDATE_KEY"
		rm -f "$2.unsigned"
		UPDATE_FILES="$UPDATE_FILES $1 $2"
	fi
}
for d in $DEVICES; do
	[ -s "$(image $d)" ] && [ -s "$(sources $d)" ] || stop "plastron's release didn't make $(image $d) and $(sources $d)"
	xz -t "$(image $d)"
	if [ $UPDATES = 1 ]; then
		set -- $(update_files $d)
		sign_list "$1" "$2"
		sign_list "$3" "$4"
	fi
done
shasum -a 256 $RELEASE_FILES $UPDATE_FILES > SHA256SUMS
printf 'ZyperOS %s (%s), plastron/ included\n' "$VERSION" "$HEAD" > BUILT-FROM
cat SHA256SUMS
ls -l

if [ $PUBLISH = 0 ]; then
	echo
	echo "Built in $OUT. Nothing was published: tools/release.sh ${KIT:+--kit \"$KIT\" }--publish does that."
	exit 0
fi
publish
