#!/bin/sh
# The sources a release's QEMU image carries beside it (scripts/release-qemu.sh):
#
#   sh qemu-sources.sh VERSION LEGAL_INFO PLASTRON OUT.tar
#
# VERSION is ZyperOS's, LEGAL_INFO the QEMU build's legal-info folder, PLASTRON this repository's
# plastron/ folder. The image is the Pixel 2's packages, at the versions of the
# same release's Pixel 2 image, whose sources tar carries their sources (the
# kernel's and U-Boot's tarballs and Buildroot's too). What only the QEMU
# image has is here, in qemu-sources/:
#
#   README.txt               what is where
#   manifest.csv             every package in the image, its version, license
#                            and source archive (legal-info's)
#   px2sim-<v>/              px2sim's source as Buildroot took it, and its
#                            license (GPL-3.0+, its kernel module GPL-2.0)
#   plastron/package/px2sim/ and its Buildroot package
#   plastron/board/qemu/     the kernel's configuration for QEMU's virt
#                            machine (linux.config, from linux-qemu.fragment),
#                            U-Boot's (u-boot.fragment), and the image's own
#                            files: its card layout, start scripts, settings
#   plastron/configs/qemu_defconfig
#   plastron/scripts/        kernel-config-qemu.sh and build-qemu-uboot.sh
#
# ZyperOS's tools/qemu-sources-check.sh then holds it against the Pixel 2's
# sources tar: every package of the image under a copyleft license has its
# source in one of the two. Runs in the build container and on a Mac alike
# (ZyperOS's tools/qemu-image-check.sh).
set -eu
VERSION=$1
LEGAL=$2
PLASTRON=$3
OUT=$4
case $OUT in /*) ;; *) OUT=$PWD/$OUT ;; esac
fail() { echo "qemu-sources: $*" >&2; exit 1; }

[ -f "$LEGAL/manifest.csv" ] || fail "no manifest.csv in $LEGAL"
# px2sim's row, "px2sim","<version>",...
PX2SIM_V=$(sed -n 's/^"px2sim","\([^"]*\)".*/\1/p' "$LEGAL/manifest.csv")
[ -n "$PX2SIM_V" ] || fail "no px2sim in $LEGAL/manifest.csv"
SRC=$LEGAL/sources/px2sim-$PX2SIM_V
LIC=$LEGAL/licenses/px2sim-$PX2SIM_V
ls "$SRC"/*.tar.gz > /dev/null 2>&1 || fail "no px2sim source archive in $SRC"
[ -f "$LIC/LICENSE" ] || fail "no px2sim license in $LIC"
# A package's version from the manifest, for the README
version() { sed -n "s/^\"$1\",\"\([^\"]*\)\".*/\1/p" "$LEGAL/manifest.csv"; }
LINUX_V=$(version linux)
LLVM_V=$(version llvm)

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
Q=$STAGE/qemu-sources
mkdir -p "$Q/px2sim-$PX2SIM_V" "$Q/plastron/package" "$Q/plastron/board" "$Q/plastron/configs" "$Q/plastron/scripts"
cp "$LEGAL/manifest.csv" "$Q/"
cp "$SRC"/*.tar.gz "$LIC/LICENSE" "$Q/px2sim-$PX2SIM_V/"
cp -R "$PLASTRON/buildroot/package/px2sim" "$Q/plastron/package/"
cp -R "$PLASTRON/buildroot/board/qemu" "$Q/plastron/board/"
cp "$PLASTRON/buildroot/configs/qemu_defconfig" "$Q/plastron/configs/"
cp "$PLASTRON/scripts/kernel-config-qemu.sh" "$PLASTRON/scripts/build-qemu-uboot.sh" "$Q/plastron/scripts/"
cp "$PLASTRON/LICENSE" "$Q/plastron/"
find "$Q" -name .DS_Store -delete

cat > "$Q/README.txt" <<EOF
The sources of ZyperOS's QEMU image, beside the release's other sources.

The image (ZyperOS-v${VERSION}-qemu.tar.xz) is the GKD Pixel 2's system built
for QEMU's arm64 virt machine by plastron's configs/qemu_defconfig. manifest.csv
lists every package in it, with its version, license and source archive.
Their sources are in the same release's ZyperOS-v${VERSION}-pixel2-sources.tar,
at the same versions: the Linux ${LINUX_V:-} and U-Boot tarballs, every
package's source and patches, and Buildroot itself.

What only the QEMU image has is here:

- px2sim ${PX2SIM_V} (GPL-3.0+; its kernel module px2sim_hw GPL-2.0), which
  simulates the Pixel 2's buttons, battery, charger, backlight and side
  lights: px2sim-${PX2SIM_V}/, its source as Buildroot took it and its license,
  and plastron/package/px2sim/, the Buildroot package that builds it.
- The kernel for the virt machine: Linux ${LINUX_V:-} without the Pixel 2's
  patches, configured by plastron/board/qemu/linux.config (made from
  linux-qemu.fragment by plastron/scripts/kernel-config-qemu.sh).
- U-Boot for the virt machine: qemu_arm64_defconfig with
  plastron/board/qemu/u-boot.fragment, built by
  plastron/scripts/build-qemu-uboot.sh.
- The image's own files: plastron/configs/qemu_defconfig and the rest of
  plastron/board/qemu/ (its card layout, start scripts and settings).

plastron is under the GNU GPL version 3 (plastron/LICENSE). LLVM ${LLVM_V:-}
(Apache-2.0 with LLVM exceptions, for Mesa's llvmpipe) is in the image too and
not in the Pixel 2's; its license asks for no source, which is at
https://github.com/llvm/llvm-project.
EOF

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
# The files in a fixed order
(cd "$STAGE" && find qemu-sources -type f | LC_ALL=C sort > "$STAGE/list" &&
	tar -cf "$OUT" -T "$STAGE/list") || fail "could not write $OUT"

# What it must hold, read back from the tar itself
names=$(tar -tf "$OUT")
for f in README.txt manifest.csv "px2sim-$PX2SIM_V/LICENSE" plastron/package/px2sim/px2sim.mk \
	plastron/package/px2sim/src/px2sim.c plastron/package/px2sim/src/kmod/px2sim_hw.c \
	plastron/board/qemu/linux.config plastron/board/qemu/linux-qemu.fragment \
	plastron/board/qemu/u-boot.fragment plastron/configs/qemu_defconfig \
	plastron/scripts/kernel-config-qemu.sh plastron/scripts/build-qemu-uboot.sh; do
	printf '%s\n' "$names" | grep -qx "qemu-sources/$f" || fail "$OUT has no qemu-sources/$f"
done
printf '%s\n' "$names" | grep -qx "qemu-sources/px2sim-$PX2SIM_V/px2sim-$PX2SIM_V\.tar\.gz" ||
	fail "$OUT has no px2sim-$PX2SIM_V.tar.gz"
echo "qemu-sources: $OUT, px2sim $PX2SIM_V and the QEMU kernel's and U-Boot's configuration ($(printf '%s\n' "$names" | wc -l | tr -d ' ') files)"
