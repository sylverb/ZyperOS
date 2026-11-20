#!/bin/sh
# Runs inside the build container (make release-qemu): QEMU's image, built as
# a release is, from an empty output folder of its own, for ZyperOS's
# tools/qemu-image.sh to publish, so a developer without plastron's build can
# run ZyperOS in QEMU (ZyperOS's docs/qemu.md). Into /work/release-qemu:
#
#   zyperos-qemu.tar.xz        sdcard.img, Image and u-boot.bin, what
#                              ZyperOS's tools/qemu-run.sh boots
#   zyperos-qemu-sources.tar   the licenses and sources of everything in it
#
# The image carries no SSH key and no private key of any kind: root's key
# comes from QEMU at every boot (board/qemu/post-build.sh), and the host keys
# are made at boot. Both are checked here on what goes out, the root
# filesystem as squashed, not trusted.
set -eu

OUTPUT=/work/release-output-qemu
RELEASE=/work/release-qemu
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

# The three files, one archive: tools/qemu-run.sh fetch checks its sha256
# against ZyperOS's tools/qemu/image.pin and takes exactly these names
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$OUTPUT/images/sdcard.img" "$OUTPUT/images/Image" "$STAGE/"
plastron_lock /work/qemu-uboot -s
cp /work/qemu-uboot/u-boot.bin "$STAGE/"
mkdir -p "$RELEASE"
rm -f "$RELEASE/zyperos-qemu.tar.xz" "$RELEASE/zyperos-qemu-sources.tar"
tar -C "$STAGE" --owner=0 --group=0 --numeric-owner -cf - sdcard.img Image u-boot.bin |
	xz -T0 -9 > "$RELEASE/zyperos-qemu.tar.xz"
tar -C "$OUTPUT" -cf "$RELEASE/zyperos-qemu-sources.tar" legal-info
rm -rf "$STAGE"
ls -l "$RELEASE/zyperos-qemu.tar.xz" "$RELEASE/zyperos-qemu-sources.tar"
