#!/bin/sh
# Runs inside the build container (make release-qemu), for a ZyperOS
# release: QEMU's image, built as the devices' release images are, from an
# empty output folder of its own, so a developer without plastron's build
# runs that release in QEMU and in ZyperOS's Simulator (ZyperOS's
# docs/qemu.md). Into /work/release, beside the devices' files:
#
#   ZyperOS-v<version>-qemu.tar.xz   sdcard.img, Image and u-boot.bin, what
#                                    ZyperOS's tools/qemu-run.sh boots
#
# ZyperOS's tools/release.sh puts it on the release with its sha256 in
# SHA256SUMS, and tools/qemu-run.sh fetch downloads it by that. This build's
# legal-info stays in its output folder, with U-Boot's and Buildroot's
# sources added as scripts/release.sh adds them to the Pixel 2's.
#
# The image carries no SSH key and no private key of any kind: root's key
# comes from QEMU at every boot (board/qemu/post-build.sh), and the host keys
# are made at boot. scripts/qemu-key-check.sh checks both on what goes out,
# the root filesystem as squashed, not trusted.
set -eu

VERSION=${1:?usage: release-qemu.sh VERSION}
NAME=ZyperOS-v$VERSION-qemu.tar.xz
OUTPUT=/work/release-output-qemu
RELEASE=/work/release
SOURCES=$OUTPUT/legal-info/sources
STAGE=/tmp/release-qemu

stop() { echo "release-qemu: $*" >&2; exit 1; }

. /src/scripts/lock.sh
plastron_lock "$OUTPUT"
rm -rf "$OUTPUT"
PLASTRON_RELEASE=1 FLAVOR=qemu PLASTRON_OUTPUT=$OUTPUT sh /src/scripts/build.sh all legal-info
sh /src/scripts/build-qemu-uboot.sh

# What gets the VM in, and no key in what ships (scripts/qemu-key-check.sh)
UNSQ=$OUTPUT/host/bin/unsquashfs
[ -x "$UNSQ" ] || UNSQ=$(command -v unsquashfs) || stop "no unsquashfs to list the root filesystem with"
sh /src/scripts/qemu-key-check.sh "$OUTPUT/target" "$OUTPUT/images/rootfs.squashfs" "$UNSQ" ||
	stop "the image is not one to publish"

# U-Boot is built outside Buildroot (scripts/build-qemu-uboot.sh), so
# legal-info has no source for it: the tarball it is built from, the
# fragment and the script, beside the rest
eval "$(grep -E '^UBOOT_VERSION=' /src/scripts/build-bootloader.sh)"
UBOOT=$SOURCES/u-boot-$UBOOT_VERSION
mkdir -p "$UBOOT/plastron"
cp "/work/dl/u-boot-$UBOOT_VERSION.tar.gz" "$UBOOT/"
cp /src/buildroot/board/qemu/u-boot.fragment /src/scripts/build-qemu-uboot.sh "$UBOOT/plastron/"
find "$SOURCES" -name .DS_Store -delete

# Buildroot itself, which legal-info never saves (scripts/release.sh says why)
BUILDROOT_VERSION=$(sed -n 's/^BUILDROOT_VERSION=//p' /src/scripts/build.sh)
mkdir -p "$OUTPUT/legal-info/buildroot-source"
cp "/work/dl/buildroot-$BUILDROOT_VERSION.tar.xz" "$OUTPUT/legal-info/buildroot-source/"

# The cores' sources are in ZyperOS's cores release (scripts/release.sh)
CORES_TAG=$(sed -n 's/^CORES_TAG=//p' /zyperos/mk/fetch-vendor.sh 2>/dev/null || true)
if [ -n "$CORES_TAG" ]; then
	cat > "$OUTPUT/legal-info/CORES-SOURCE.txt" <<-EOF
	The libretro cores in ZyperOS/cores that mk/fetch-vendor.sh fetches are
	not built here, so their sources are not in this archive. They are in
	ZyperOS's cores release, $CORES_TAG:

	  https://github.com/tarikbc/ZyperOS/releases/tag/$CORES_TAG

	with the exact binaries, and for each core under a GPL or an LGPL an
	archive of its source at the commit of that binary,
	<core>-<commit>-source.tar.gz, all in its SHA256SUMS.
	EOF
fi

# The three files, one archive: tools/qemu-run.sh fetch takes exactly
# these names, after the archive's sha256 matches the release's SHA256SUMS
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$OUTPUT/images/sdcard.img" "$OUTPUT/images/Image" "$STAGE/"
plastron_lock /work/qemu-uboot -s
cp /work/qemu-uboot/u-boot.bin "$STAGE/"
mkdir -p "$RELEASE"
rm -f "$RELEASE/$NAME"
tar -C "$STAGE" --owner=0 --group=0 --numeric-owner -cf - sdcard.img Image u-boot.bin |
	xz -T0 -9 > "$RELEASE/$NAME"
rm -rf "$STAGE"
ls -l "$RELEASE/$NAME"
