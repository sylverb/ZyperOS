#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The published QEMU image's way to a developer (docs/qemu.md, "Setting
# it up"), without the network, QEMU or an image:
#
#  - tools/qemu-run.sh fetch, from a stand-in archive on this computer
#    (file://): no pin says so and changes nothing; the pinned archive goes
#    into image/ with FETCHED; again, nothing is downloaded; a download cut
#    short goes on; an archive with another sha256, or with any other file
#    in it, changes nothing; an image/ fetch did not put there stays unless
#    --force.
#  - tools/qemu/image.pin: empty, or a qemu-* tag and a sha256.
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
pin() { printf 'tag=%s\nsha256=%s\n' "$1" "$2" > "$T/pin"; }
fetch() { # fetch ARCHIVE [ARGS]: qemu-run.sh fetch, its output in $T/out
	a=$1
	shift
	ZYPEROS_QEMU_HOME=$T/home ZYPEROS_QEMU_IMAGE_PIN=$T/pin ZYPEROS_QEMU_IMAGE_URL=file://$a \
		sh "$RUN" fetch "$@" > "$T/out" 2>&1
}
said() { grep -q "$1" "$T/out"; }
same_image() { cmp -s "$T/src/sdcard.img" "$T/home/image/sdcard.img" && cmp -s "$T/src/Image" "$T/home/image/Image" &&
	cmp -s "$T/src/u-boot.bin" "$T/home/image/u-boot.bin"; }

echo "fetch"
pin '' ''
if ! fetch "$T/good.tar.xz" && said "no published image yet" && [ ! -e "$T/home/image" ]; then
	ok "no pin: says so, makes nothing"
else bad "no pin: $(cat "$T/out")"; fi

pin qemu-check 1234
if ! fetch "$T/good.tar.xz" && said "is not one" && [ ! -e "$T/home/image" ]; then
	ok "a pin whose sha256 is not one is refused"
else bad "a bad sha256 in the pin: $(cat "$T/out")"; fi

pin '../x' "$GOOD"
if ! fetch "$T/good.tar.xz" && said "not a qemu-<date> one" && [ ! -e "$T/home/image" ]; then
	ok "a pin whose tag is not a qemu-* one is refused"
else bad "a bad tag in the pin: $(cat "$T/out")"; fi

pin qemu-check "$GOOD"
if fetch "$T/good.tar.xz" && same_image && [ "$(cat "$T/home/image/FETCHED")" = "qemu-check $GOOD" ] &&
	[ -z "$(ls "$T/home/download")" ] && [ ! -e "$T/home/image.new" ]; then
	ok "the pinned archive: its three files in image/, FETCHED, nothing left over"
else bad "the pinned archive: $(cat "$T/out")"; fi

if fetch "$T/nowhere.tar.xz" && said "is qemu-check already"; then
	ok "again: nothing downloaded"
else bad "again: $(cat "$T/out")"; fi

# Cut short: the first part already there, as curl left it
head -c 1000 "$T/good.tar.xz" > "$T/home/download/qemu-check.tar.xz"
if fetch "$T/good.tar.xz" --force && same_image && [ -z "$(ls "$T/home/download")" ]; then
	ok "a download cut short goes on from where it stopped"
else bad "a download cut short: $(cat "$T/out")"; fi

pin qemu-other "$(sha "$T/extra.tar.xz" | tr 0-9a-f 1-9a-f0)"
if ! fetch "$T/extra.tar.xz" && said "is not the one" && same_image &&
	[ "$(cat "$T/home/image/FETCHED")" = "qemu-check $GOOD" ] && [ -z "$(ls "$T/home/download")" ]; then
	ok "another sha256 than the pin's: refused, image/ as it was, the download gone"
else bad "another sha256: $(cat "$T/out")"; fi

pin qemu-other "$(sha "$T/extra.tar.xz")"
if ! fetch "$T/extra.tar.xz" && said "not Image sdcard.img u-boot.bin" && same_image &&
	[ ! -e "$T/home/image/extra.txt" ] && [ -z "$(ls "$T/home/download")" ]; then
	ok "an archive with another file in it: refused, image/ as it was"
else bad "an archive with another file: $(cat "$T/out")"; fi

rm -f "$T/home/image/FETCHED"
pin qemu-check "$GOOD"
if ! fetch "$T/good.tar.xz" && said "fetch did not put there" && [ ! -e "$T/home/image/FETCHED" ]; then
	ok "an image/ fetch did not put there stays"
else bad "someone else's image/: $(cat "$T/out")"; fi
if fetch "$T/good.tar.xz" --force && same_image && [ -f "$T/home/image/FETCHED" ]; then
	ok "--force replaces it"
else bad "--force: $(cat "$T/out")"; fi

echo "the pin"
REAL=$ROOT/tools/qemu/image.pin
tag=$(sed -n 's/^tag=//p' "$REAL")
sum=$(sed -n 's/^sha256=//p' "$REAL")
if { [ -z "$tag" ] && [ -z "$sum" ]; } ||
	{ printf '%s\n' "$tag" | grep -qx 'qemu-[0-9A-Za-z.-]*' && printf '%s\n' "$sum" | grep -qx '[0-9a-f]\{64\}'; }; then
	ok "tools/qemu/image.pin: ${tag:-no image published yet}"
else bad "tools/qemu/image.pin names tag '$tag' and sha256 '$sum'"; fi

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
