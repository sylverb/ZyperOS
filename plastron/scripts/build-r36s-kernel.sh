#!/bin/sh
# The R36S's kernel and device tree, a start (docs/devices/r36s.md): the
# GKD Pixel 2's kernel, kernel.org 7.1.2 with board/px2/patches/linux (the
# Pixel 2's ROCKNIX patches; the R36S's panel and joypad are among them),
# its config with board/r36s/linux-r36s.fragment merged in, and ROCKNIX's
# R36S device tree from board/r36s/dts on the Pixel 2's rk3326.dtsi. Runs in
# the builder (make r36s-kernel), with Debian's cross compiler, so it needs
# no Buildroot output of its own. Output in /work/r36s-kernel/images: Image,
# rk3326-gameconsole-r36s.dtb and the config it was built with.
#
# Nothing boots this yet: there is no R36S image (its U-Boot, its card and
# the launcher's device file are the docs' to-do).
set -eu

LINUX=linux-7.1.2
# Buildroot's download of it, which every Pixel 2 build makes
TARBALL=/work/buildroot-2026.02.3/dl/linux/$LINUX.tar.xz
DIR=/work/r36s-kernel
SRC=$DIR/$LINUX
OUT=$DIR/out
IMAGES=$DIR/images
JOBS=${JOBS:-4}
export KBUILD_BUILD_VERSION=1 KBUILD_BUILD_USER=plastron KBUILD_BUILD_HOST=plastron
export KBUILD_BUILD_TIMESTAMP="Sun Nov 22 00:00:00 UTC 2026"

. /src/scripts/lock.sh
plastron_lock "$DIR"
if [ ! -f "$TARBALL" ]; then
	echo "r36s-kernel: no $TARBALL: build the Pixel 2's image once (make), which downloads it" >&2
	exit 1
fi

rm -rf "$SRC" "$OUT" "$IMAGES"
mkdir -p "$SRC" "$OUT" "$IMAGES"
tar -C "$SRC" --strip-components=1 -xJf "$TARBALL"
cd "$SRC"
# In Buildroot's order, by name
for p in $(ls /src/buildroot/board/px2/patches/linux/*.patch | sort); do
	patch -p1 -s --batch < "$p"
done
# ROCKNIX's rk3326.dtsi over mainline's, as for the Pixel 2, and the R36S's
# three files
D=arch/arm64/boot/dts/rockchip
cp /src/buildroot/board/px2/dts/rk3326.dtsi "$D/"
cp /src/buildroot/board/r36s/dts/* "$D/"
echo 'dtb-$(CONFIG_ARCH_ROCKCHIP) += rk3326-gameconsole-r36s.dtb' >> "$D/Makefile"

k() { make -s -C "$SRC" O="$OUT" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j"$JOBS" "$@"; }
# The Pixel 2's config names its kernel symbols list by the build
# container's path, which is this one's too
cp /src/buildroot/board/px2/linux.config "$OUT/.config"
ARCH=arm64 scripts/kconfig/merge_config.sh -m -O "$OUT" "$OUT/.config" \
	/src/buildroot/board/r36s/linux-r36s.fragment
k olddefconfig
# Each line of the fragment as it came out
bad=0
grep '^CONFIG_' /src/buildroot/board/r36s/linux-r36s.fragment | while read -r line; do
	grep -qx "$line" "$OUT/.config" || { echo "r36s-kernel: $line did not hold" >&2; exit 1; }
done || bad=1
[ $bad = 0 ] || exit 1
k rockchip/rk3326-gameconsole-r36s.dtb
k Image
cp "$OUT/arch/arm64/boot/Image" "$OUT/$D/rk3326-gameconsole-r36s.dtb" "$IMAGES/"
cp "$OUT/.config" "$IMAGES/linux.config"
ls -l "$IMAGES"
