#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# A release's QEMU image on its way to a developer (docs/qemu.md, "Setting
# it up"), without the network, QEMU or an image:
#
#  - tools/qemu-run.sh fetch, from a stand-in release on this computer (a
#    folder with SHA256SUMS and ZyperOS-v<version>-qemu.tar.xz, file://): a
#    release without the image says so and changes nothing; the image goes
#    into image/ with FETCHED once its sha256 matches the release's
#    SHA256SUMS; again, nothing is downloaded; a download cut short goes on;
#    another sha256, any other file in the archive, two images in one
#    SHA256SUMS or another --version change nothing; an image/ fetch did not
#    put there stays unless --force.
#  - tools/release.sh puts the image and its sources tar in a release's files
#    and its SHA256SUMS.
#  - The sources tar (plastron's scripts/qemu-sources.sh, on a stand-in
#    legal-info and the real plastron files) lists px2sim's source, license
#    and package and the QEMU kernel's and U-Boot's configuration, and is
#    refused without px2sim's source; tools/qemu-sources-check.sh passes it
#    beside a Pixel 2 sources tar with the same packages, and names a
#    copyleft package whose version the Pixel 2's does not have.
#  - The key: start hands QEMU the public half of the key it uses, the
#    VMs' own made on the first start when there is no other, as the fw_cfg
#    file the image reads (a stand-in qemu-system-aarch64 records the
#    arguments; macOS only, as qemu-run.sh is). The image's S49sshkey puts
#    that file where /root/.ssh points, before dropbear starts, and the
#    kernel has fw_cfg in sysfs; the image's post-build leaves no key in it.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
RUN=$ROOT/tools/qemu-run.sh
QEMU_BOARD=$ROOT/plastron/buildroot/board/qemu
SSHKEY=$QEMU_BOARD/rootfs-overlay/etc/init.d/S49sshkey
FWCFG=opt/org.zyperos/authorized_keys
T=$(mktemp -d "${TMPDIR:-/tmp}/qemu-image-check.XXXXXX")
trap 'rm -rf "$T"' EXIT
fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }
sha() { shasum -a 256 "$1" | cut -c1-64; }

# A stand-in image, and archives of it
mkdir -p "$T/src"
printf 'card %s\n' "$(date +%s)" > "$T/src/sdcard.img"
head -c 300000 /dev/urandom >> "$T/src/sdcard.img"
echo kernel > "$T/src/Image"
echo u-boot > "$T/src/u-boot.bin"
(cd "$T/src" && tar -cJf "$T/good.tar.xz" sdcard.img Image u-boot.bin)
echo extra > "$T/src/extra.txt"
(cd "$T/src" && tar -cJf "$T/extra.tar.xz" sdcard.img Image u-boot.bin extra.txt)
GOOD=$(sha "$T/good.tar.xz")
IMG=ZyperOS-v9.9.9-qemu.tar.xz
# release ARCHIVE [SHA256SUMS lines...]: the stand-in release's folder, the
# archive as its image, and SHA256SUMS with the lines given (the archive's
# own when none are)
release() {
	rm -rf "$T/rel"
	mkdir -p "$T/rel"
	[ -z "$1" ] || cp "$1" "$T/rel/$IMG"
	a=$1
	shift
	if [ $# -eq 0 ] && [ -n "$a" ]; then
		printf '%s  %s\n' "$(sha "$a")" "$IMG" > "$T/rel/SHA256SUMS"
	else
		printf '%s\n' "$@" > "$T/rel/SHA256SUMS"
	fi
	printf '%s  ZyperOS-v9.9.9-pixel2.img.xz\n' "$(printf x | shasum -a 256 | cut -c1-64)" >> "$T/rel/SHA256SUMS"
}
fetch() { # fetch [ARGS]: qemu-run.sh fetch from the stand-in release, its output in $T/out
	ZYPEROS_QEMU_HOME=$T/home ZYPEROS_QEMU_RELEASE_URL=file://$T/rel \
		sh "$RUN" fetch "$@" > "$T/out" 2>&1
}
said() { grep -q "$1" "$T/out"; }
same_image() { cmp -s "$T/src/sdcard.img" "$T/home/image/sdcard.img" && cmp -s "$T/src/Image" "$T/home/image/Image" &&
	cmp -s "$T/src/u-boot.bin" "$T/home/image/u-boot.bin"; }
nothing_left() { [ -z "$(ls "$T/home/download" 2> /dev/null)" ] && [ ! -e "$T/home/image.new" ]; }

echo "fetch"
release '' '0000000000000000000000000000000000000000000000000000000000000000  ZyperOS-v9.9.9-brick.img.xz'
if ! fetch && said "has no QEMU image" && [ ! -e "$T/home/image" ] && nothing_left; then
	ok "a release without the image: says so, makes nothing"
else bad "a release without the image: $(cat "$T/out")"; fi

release '' "$GOOD  ZyperOS-v9.9.9/../x-qemu.tar.xz" "$GOOD  ../ZyperOS-v9.9.9-qemu.tar.xz"
if ! fetch && said "has no QEMU image" && [ ! -e "$T/home/image" ]; then
	ok "a name that is not ZyperOS-v<version>-qemu.tar.xz is no image"
else bad "a strange name: $(cat "$T/out")"; fi

release "$T/good.tar.xz"
if fetch && same_image && [ "$(cat "$T/home/image/FETCHED")" = "v9.9.9 $GOOD" ] && nothing_left &&
	said "matches v9.9.9's SHA256SUMS"; then
	ok "the release's image: its sha256 checked, its three files in image/, FETCHED, nothing left over"
else bad "the release's image: $(cat "$T/out")"; fi

rm -f "$T/rel/$IMG"
if fetch && said "is v9.9.9's already"; then
	ok "again: nothing downloaded"
else bad "again: $(cat "$T/out")"; fi

# Cut short: the first part already there, as curl left it
release "$T/good.tar.xz"
head -c 1000 "$T/good.tar.xz" > "$T/home/download/$IMG"
if fetch --force && same_image && nothing_left; then
	ok "a download cut short goes on from where it stopped"
else bad "a download cut short: $(cat "$T/out")"; fi

release "$T/extra.tar.xz" "$(sha "$T/good.tar.xz")  $IMG"
if ! fetch --force && said "is not the one" && same_image &&
	[ "$(cat "$T/home/image/FETCHED")" = "v9.9.9 $GOOD" ] && nothing_left; then
	ok "another sha256 than SHA256SUMS's: refused, image/ as it was, the download gone"
else bad "another sha256: $(cat "$T/out")"; fi

release "$T/extra.tar.xz"
if ! fetch --force && said "not Image sdcard.img u-boot.bin" && same_image &&
	[ ! -e "$T/home/image/extra.txt" ] && nothing_left; then
	ok "an archive with another file in it: refused, image/ as it was"
else bad "an archive with another file: $(cat "$T/out")"; fi

release "$T/good.tar.xz" "$GOOD  $IMG" "$GOOD  ZyperOS-v9.9.8-qemu.tar.xz"
if ! fetch --force && said "more than one QEMU image" && same_image; then
	ok "two images in one SHA256SUMS: refused"
else bad "two images: $(cat "$T/out")"; fi

release "$T/good.tar.xz"
if ! fetch --version 1.2.3 --force && said "not v1.2.3's" && same_image; then
	ok "--version another release's: refused"
else bad "--version: $(cat "$T/out")"; fi

rm -f "$T/home/image/FETCHED"
if ! fetch && said "fetch did not put there" && [ ! -e "$T/home/image/FETCHED" ]; then
	ok "an image/ fetch did not put there stays"
else bad "someone else's image/: $(cat "$T/out")"; fi
if fetch --force && same_image && [ -f "$T/home/image/FETCHED" ]; then
	ok "--force replaces it"
else bad "--force: $(cat "$T/out")"; fi

echo "the release"
REL=$ROOT/tools/release.sh
grep -q 'qemu_image() { echo "ZyperOS-$TAG-qemu.tar.xz"; }' "$REL" &&
	grep -q '\[ $QEMU = 0 \] || RELEASE_FILES="$RELEASE_FILES $(qemu_image) ' "$REL" &&
	grep -q 'shasum -a 256 $RELEASE_FILES $UPDATE_FILES > SHA256SUMS' "$REL" &&
	ok "tools/release.sh puts ZyperOS-v<version>-qemu.tar.xz in the release's files and SHA256SUMS" ||
	bad "tools/release.sh does not put the QEMU image in the release's files and SHA256SUMS"
grep -q 'release-qemu VERSION="$VERSION"' "$REL" && grep -q 'NAME=ZyperOS-v$VERSION-qemu.tar.xz' "$ROOT/plastron/scripts/release-qemu.sh" &&
	ok "and builds it with plastron's release-qemu, which names it so" ||
	bad "tools/release.sh and plastron's release-qemu.sh name the image differently"

grep -q 'RELEASE_FILES="$RELEASE_FILES $(qemu_image) $(qemu_sources)"' "$REL" &&
	grep -q 'GET="$GET $(qemu_image) $(qemu_sources)"' "$REL" &&
	grep -q 'SOURCES_NAME=ZyperOS-v$VERSION-qemu-sources.tar' "$ROOT/plastron/scripts/release-qemu.sh" &&
	grep -q 'qemu_sources() { echo "ZyperOS-$TAG-qemu-sources.tar"; }' "$REL" &&
	ok "and its sources tar, ZyperOS-v<version>-qemu-sources.tar, beside it" ||
	bad "tools/release.sh does not put the QEMU sources tar in the release, under the name release-qemu.sh gives it"

echo "the sources"
QS=$ROOT/plastron/scripts/qemu-sources.sh
QC=$ROOT/tools/qemu-sources-check.sh
L=$T/legal
mkdir -p "$L/sources/px2sim-1.0" "$L/licenses/px2sim-1.0"
cat > "$L/manifest.csv" <<-'EOF2'
	"PACKAGE","VERSION","LICENSE","LICENSE FILES","SOURCE ARCHIVE","SOURCE SITE","DEPENDENCIES WITH LICENSES"
	"busybox","1.37.0","GPL-2.0","LICENSE","busybox-1.37.0.tar.bz2","https://busybox.net","gcc-final [GPL-3.0]"
	"linux","7.1.2","GPL-2.0","COPYING","linux-7.1.2.tar.xz","https://cdn.kernel.org","gcc-final [GPL-3.0]"
	"llvm","21.1.8","Apache-2.0 with exceptions","LICENSE.TXT","llvm-21.1.8.src.tar.xz","https://github.com","gcc-final [GPL-3.0]"
	"px2sim","1.0","GPL-3.0+ (px2sim), GPL-2.0 (px2sim_hw)","LICENSE","px2sim-1.0.tar.gz","/src","linux [GPL-2.0]"
EOF2
(cd "$T" && mkdir -p px2sim-1.0 && echo src > px2sim-1.0/px2sim.c && tar -czf "$L/sources/px2sim-1.0/px2sim-1.0.tar.gz" px2sim-1.0)
echo "GPL-3.0" > "$L/licenses/px2sim-1.0/LICENSE"
if sh "$QS" 9.9.9 "$L" "$ROOT/plastron" "$T/qs.tar" > "$T/out" 2>&1 &&
	tar -tf "$T/qs.tar" | grep -qx 'qemu-sources/px2sim-1.0/px2sim-1.0.tar.gz' &&
	tar -tf "$T/qs.tar" | grep -qx 'qemu-sources/plastron/package/px2sim/src/kmod/px2sim_hw.c' &&
	tar -tf "$T/qs.tar" | grep -qx 'qemu-sources/plastron/board/qemu/linux.config' &&
	tar -tf "$T/qs.tar" | grep -qx 'qemu-sources/plastron/board/qemu/u-boot.fragment' &&
	tar -xOf "$T/qs.tar" qemu-sources/README.txt | grep -q 'ZyperOS-v9.9.9-pixel2-sources.tar'; then
	ok "qemu-sources.sh: px2sim's source, license and package, the QEMU kernel's and U-Boot's configuration, a README"
else bad "qemu-sources.sh: $(cat "$T/out")"; fi
mv "$L/sources/px2sim-1.0" "$T/px2sim-src-away"
if ! sh "$QS" 9.9.9 "$L" "$ROOT/plastron" "$T/qs-none.tar" > "$T/out" 2>&1 && said "no px2sim source archive"; then
	ok "qemu-sources.sh without px2sim's source: refused"
else bad "qemu-sources.sh without px2sim's source: $(cat "$T/out")"; fi
mv "$T/px2sim-src-away" "$L/sources/px2sim-1.0"
pixel2() { # pixel2 "PKG VER LICENSE"...: a stand-in Pixel 2 sources tar
	rm -rf "$T/p2"
	mkdir -p "$T/p2/legal-info"
	echo '"PACKAGE","VERSION","LICENSE"' > "$T/p2/legal-info/manifest.csv"
	for r in "$@"; do
		set -- $r
		printf '"%s","%s","%s"\n' "$1" "$2" "$3" >> "$T/p2/legal-info/manifest.csv"
	done
	(cd "$T/p2" && tar -cf "$T/p2.tar" legal-info)
}
pixel2 "busybox 1.37.0 GPL-2.0" "linux 7.1.2 GPL-2.0" "jackswitch 1.0 GPL-3.0"
if sh "$QC" "$T/qs.tar" "$T/p2.tar" > "$T/out" 2>&1 &&
	said "4 packages: 2 with the Pixel 2's sources, 1 in the QEMU sources, 1 under licenses that ask for no source"; then
	ok "qemu-sources-check: every package's source in one of the two tars"
else bad "qemu-sources-check, all there: $(cat "$T/out")"; fi
pixel2 "busybox 1.36.1 GPL-2.0" "linux 7.1.2 GPL-2.0"
if ! sh "$QC" "$T/qs.tar" "$T/p2.tar" > "$T/out" 2>&1 && said "busybox 1.37.0 (GPL-2.0)"; then
	ok "qemu-sources-check: a GPL package the Pixel 2's has at another version is named"
else bad "qemu-sources-check, another version: $(cat "$T/out")"; fi

echo "the key"
grep -qx 'CONFIG_FW_CFG_SYSFS=y' "$QEMU_BOARD/linux-qemu.fragment" &&
	grep -qx 'CONFIG_FW_CFG_SYSFS=y' "$QEMU_BOARD/linux.config" &&
	ok "the kernel has fw_cfg in sysfs (fragment and linux.config)" ||
	bad "CONFIG_FW_CFG_SYSFS=y is not in linux-qemu.fragment and linux.config"
grep -q "by_name/$FWCFG/raw" "$SSHKEY" && grep -q "name=$FWCFG," "$RUN" &&
	ok "S49sshkey reads the fw_cfg file qemu-run.sh hands QEMU" ||
	bad "S49sshkey and qemu-run.sh name different fw_cfg files"
first=$(printf '%s\n' S49sshkey S50dropbear | LC_ALL=C sort | head -1)
[ "$first" = S49sshkey ] && ok "S49sshkey runs before S50dropbear" || bad "S49sshkey runs after S50dropbear"
grep -q 'rm -rf "$TARGET_DIR/root/.ssh"' "$QEMU_BOARD/post-build.sh" &&
	grep -q 'ln -s /run/ssh-root "$TARGET_DIR/root/.ssh"' "$QEMU_BOARD/post-build.sh" &&
	grep -q 'DIR=${SSH_ROOT:-/run/ssh-root}' "$SSHKEY" &&
	ok "the image's post-build takes out root's .ssh and points it at S49sshkey's /run/ssh-root" ||
	bad "board/qemu/post-build.sh leaves root's .ssh, or points it somewhere S49sshkey does not write"

echo "ssh-ed25519 AAAA-stand-in check" > "$T/fwcfg"
FWCFG_KEY=$T/fwcfg SSH_ROOT=$T/sshroot sh "$SSHKEY" start > "$T/out" 2>&1
mode() { ls -ld "$1" | cut -c1-10; }
if cmp -s "$T/fwcfg" "$T/sshroot/authorized_keys" && [ "$(mode "$T/sshroot")" = drwx------ ] &&
	[ "$(mode "$T/sshroot/authorized_keys")" = -rw------- ] && said "1 key(s) from QEMU"; then
	ok "S49sshkey: QEMU's key into authorized_keys, the folder 700 and the file 600"
else bad "S49sshkey with a key: $(cat "$T/out")"; fi
rm -rf "$T/sshroot"
FWCFG_KEY=$T/none SSH_ROOT=$T/sshroot sh "$SSHKEY" start > "$T/out" 2>&1
if [ ! -e "$T/sshroot/authorized_keys" ] && said "QEMU handed no key"; then
	ok "S49sshkey with no key from QEMU: none, and it says so"
else bad "S49sshkey with no key: $(cat "$T/out")"; fi

# start, as far as QEMU: a stand-in that records its arguments and exits, so
# start stops at "QEMU exited"
if [ "$(uname -s)" = Darwin ] && command -v ssh-keygen > /dev/null; then
	mkdir -p "$T/bin" "$T/home2"
	cat > "$T/bin/qemu-system-aarch64" <<-EOF
	#!/bin/sh
	[ "\$1" = -display ] && exit 0
	printf '%s\n' "\$@" > "$T/qemu-args"
	EOF
	printf '#!/bin/sh\nexit 0\n' > "$T/bin/qemu-img"
	chmod +x "$T/bin/qemu-system-aarch64" "$T/bin/qemu-img"
	start() { # start [ENV...]: qemu-run.sh start on the stand-in image
		rm -f "$T/qemu-args"
		env PATH="$T/bin:$PATH" HOME=$T/home2 ZYPEROS_QEMU_HOME=$T/home2/vms "$@" \
			sh "$RUN" start -n check --image "$T/src" --no-test-rom > "$T/out" 2>&1
	}
	fwcfg_file() { grep -o "^name=$FWCFG,file=.*" "$T/qemu-args" 2> /dev/null | sed 's/^[^,]*,file=//'; }
	start
	VMKEY=$T/home2/vms/ssh/zyperos_qemu
	f=$(fwcfg_file)
	if [ -f "$VMKEY" ] && [ -n "$f" ] && cmp -s "$f" "$VMKEY.pub" && said "made $VMKEY"; then
		ok "start with no key: makes the VMs' own and hands QEMU its public half"
	else bad "start with no key: $(cat "$T/out") $(cat "$T/qemu-args" 2> /dev/null | tr '\n' ' ')"; fi
	ssh-keygen -q -t ed25519 -N '' -f "$T/mykey" < /dev/null
	ssh-keygen -y -f "$T/mykey" > "$T/mykey.expect"
	rm -f "$T/mykey.pub"
	start PX2_KEY="$T/mykey"
	f=$(fwcfg_file)
	if [ -n "$f" ] && [ "$(cut -d' ' -f1-2 "$f")" = "$(cut -d' ' -f1-2 "$T/mykey.expect")" ]; then
		ok "start with PX2_KEY and no .pub beside it: the public half read from the key"
	else bad "start with PX2_KEY: $(cat "$T/out")"; fi
else
	echo "  skip start's key (not macOS, or no ssh-keygen)"
fi

echo
if [ $fails -eq 0 ]; then
	echo "qemu-image-check: all passed"
else
	echo "qemu-image-check: $fails failed"
	exit 1
fi
