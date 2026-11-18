#!/bin/sh
# Runs inside plastron's build image for tools/px2-payload.sh: plastron's SDK
# at /sdk and this tree at /zyperos, both read-only but for build/px2 and
# diatom/build/pixel2, and the cores at /zyperos-vendor. The same steps as
# plastron's zyperos and diatom packages (buildroot/package/zyperos,
# buildroot/package/diatom), with the SDK's compiler and sysroot in place of
# Buildroot's: mk/cross.mk for the launcher, Muse, portkeys, System
# Update's program and p2d (NES Player 2's server), Diatom's own Makefile,
# then mk/payload.sh.
set -eu

MANIFEST=/sdk/share/plastron/sdk
[ -f "$MANIFEST" ] || { echo "px2-payload: /sdk is not a whole plastron SDK (no $MANIFEST)" >&2; exit 1; }
target=$(sed -n 's/^target=//p' "$MANIFEST")
sed 's/^/px2-payload: sdk /' "$MANIFEST"

# As Buildroot's TARGET_MAKE_ENV: its pkg-config, which answers for the sysroot
export PATH=/sdk/bin:$PATH
CC=/sdk/bin/$target-gcc
SYSROOT=/sdk/$target/sysroot
STRIP=/sdk/bin/$target-strip
JOBS=${JOBS:-4}

cd /zyperos
VERSION=$(sed -n 's/^VERSION ?= //p' Makefile)
B=build/px2

# The ScreenScraper pair into a header, read in the shell so make prints
# neither (mk/cross.mk, creds)
SS_DEVID="$(sed -n 's/^SS_DEVID=//p' /zyperos-ss.env 2> /dev/null | head -1)" \
SS_DEVPASS="$(sed -n 's/^SS_DEVPASS=//p' /zyperos-ss.env 2> /dev/null | head -1)" \
	make -s -f mk/cross.mk BUILD=$B creds
# The Pixel 2's programs (mk/cross.mk's device-programs: the launcher, Muse
# and the helpers config/device/pixel2.mk lists), and Diatom for its port
make -j"$JOBS" -f mk/cross.mk CC="$CC" SYSROOT="$SYSROOT" DEVICE=pixel2 BUILD=$B \
	VERSION="$VERSION" device-programs
PORT=$(sed -n 's/^ZYDEV_DIATOM_PORT = *//p' config/device/pixel2.mk)
make -j"$JOBS" -C diatom PORT="$PORT" CC="$CC"

# The programs alone (tools/px2-payload.sh --programs): stripped, as the
# payload's are, where ZyperOS/ has them
if [ -n "${PROGRAMS_ONLY:-}" ]; then
	P=$B/programs/ZyperOS
	rm -rf "$B/programs"
	mkdir -p "$P"
	for f in zyperos.elf muse portkeys zyperos-update zyhome p2d; do
		[ -f "$B/$f" ] && "$STRIP" -o "$P/$f" "$B/$f"
	done
	"$STRIP" -o "$P/diatom" "diatom/build/$PORT/diatom"
	chmod +x "$P"/*
	exit 0
fi

DEVICE=pixel2 OUT=/zyperos/$B/payload VERSION="$VERSION" \
	ZYPEROS_ELF=$B/zyperos.elf MUSE_ELF=$B/muse PORTKEYS_ELF=$B/portkeys ZYHOME_ELF=$B/zyhome \
	UPDATE_ELF=$B/zyperos-update P2D_ELF=$B/p2d DIATOM_ELF="diatom/build/$PORT/diatom" \
	VENDOR_DIR=/zyperos-vendor sh mk/payload.sh
# The image's tools for SSH (plastron's zyperos.mk builds them for /usr/bin),
# beside the payload rather than in it: tools/px2 copies px2-perf to a device
# whose image is older than it
for t in px2-shot px2-input px2-perf; do
	"$CC" -O2 -Wall -Wextra -o "$B/$t" "tools/$t.c" -lm
	"$STRIP" "$B/$t"
done
# Stripped, as plastron's image strips every program it carries
for f in zyperos.elf muse portkeys zyperos-update zyhome p2d diatom; do
	"$STRIP" "$B/payload/ZyperOS/$f"
done
