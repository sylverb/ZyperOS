#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# The Shader list's pictures (res/shaderdemo, tools/shaderdemo.c) beside what
# a Pixel 2 draws: Diatom on the device, fed the same scene by the stub core
# (diatom/test/stubcore.c, STUBCORE_IMAGE), through each filter, and its panel
# shot (px2-shot); then tools/shaderdemo-compare.py sets each shot beside what
# tools/shaderdemo.c --rgb565 makes of that scene, and says how far apart they
# are. For when Diatom's GLSL changes (check-shaderdemo says so): port the
# change, make shaderdemo, and run this before recording it.
#
#     tools/shaderdemo-device.sh OUT_DIR
#
# Needs a Pixel 2 on the cable with the 32 GB test card (tools/px2's card
# rule), Developer SSH, Docker with plastron's SDK (to build the stub core for
# it, as tools/px2-payload.sh does), and build-native/shaderdemo. Diatom runs
# as a port, the only way it has the panel with the launcher gone: a script in
# Roms/Ports for a minute, opened by px2-open and removed after. SSH drops
# for a second as a port starts and ends (portkeys), so this reconnects.
#
# Holds /tmp/px2-lock (or PX2_LOCK) the whole run, with an owner file.
set -u

OUT=${1:?usage: tools/shaderdemo-device.sh OUT_DIR}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
HOST=${PX2_HOST:-10.42.0.1}
KEY=${PX2_KEY:-$HOME/.ssh/zyperos_pixel2}
LOCK=${PX2_LOCK:-/tmp/px2-lock}
TEST_CARD_SECTORS=62333952
D=/tmp/shaderdemo-device
PORT_SCRIPT="/mnt/SDCARD/Roms/Ports/Shader Compare.sh"
OPTS="-o ConnectTimeout=8 -o BatchMode=yes -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR"

die() { echo "shaderdemo-device: $*" >&2; exit 1; }
# shellcheck disable=SC2086
rsh() { ssh $OPTS -i "$KEY" "root@$HOST" "$@"; }

[ -x "$REPO/build-native/shaderdemo" ] || die "no build-native/shaderdemo (make shaderdemo)"
mkdir -p "$OUT/want" "$OUT/shot" || exit 1
"$REPO/build-native/shaderdemo" --rgb565 "$OUT/want" || die "shaderdemo --rgb565 failed"

# The stub core, built by the SDK's compiler
sdk=${PX2_SDK:-}
[ -n "$sdk" ] || sdk=$(docker volume ls -q --filter label=plastron.sdk=px2 | while read -r v; do
	echo "$(docker volume inspect -f '{{.CreatedAt}}' "$v") $v"; done | sort | tail -1 | cut -d' ' -f2)
[ -n "$sdk" ] || die "no plastron SDK volume (make sdk in plastron)"
docker run --rm -v "$sdk:/sdk:ro" -v "$REPO/diatom:/d:ro" -v "$OUT:/out" "${PLASTRON_IMAGE:-plastron-builder}" sh -c \
	'export PATH=/sdk/bin:$PATH; t=$(sed -n "s/^target=//p" /sdk/share/plastron/sdk);
	 /sdk/bin/$t-gcc -std=gnu11 -O2 -I/d/src -shared -fPIC -o /out/stubcore.so /d/test/stubcore.c -lm' ||
	die "could not build the stub core with $sdk"

# The lock, for the whole run
mkdir "$LOCK" 2> /dev/null || { sed 's/^/shaderdemo-device: held: /' "$LOCK/owner" >&2; exit 75; }
trap 'rsh "rm -f \"$PORT_SCRIPT\"" < /dev/null 2> /dev/null; rm -rf "$LOCK"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
printf 'pid=%s\nwho=tools/shaderdemo-device.sh (%s)\nsince=%s\nwhat=Diatom as a port, each filter shot, about 2 min\n' \
	$$ "$PWD" "$(date '+%Y-%m-%d %H:%M:%S')" > "$LOCK/owner"

size=$(rsh 'cat /sys/block/mmcblk0/size' < /dev/null) || die "cannot reach $HOST over SSH"
[ "$size" = "$TEST_CARD_SECTORS" ] || die "the card in is not the 32 GB test card ($size sectors)"
rsh 'cat /mnt/SDCARD/ZyperOS/VERSION' < /dev/null | sed 's/^/shaderdemo-device: ZyperOS /'

# What the device's Diatom draws with: its GLSL, as text in the binary, is
# this tree's (diatom/port/gles_draw.h, its shaders' whole lines), or the shots
# say nothing about it
python3 - "$REPO/diatom/port/gles_draw.h" > "$OUT/glsl.txt" <<'EOF' || die "could not read the GLSL"
import re, sys
lines = open(sys.argv[1], encoding="utf-8").read().splitlines()
a = next(i for i, l in enumerate(lines) if l.startswith("static const char *VERTEX_SHADER"))
b = next(i for i, l in enumerate(lines) if l.startswith("static const char *SOLID_SHADER"))
fresh = True
for l in lines[a:b]:
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', l)
    if not lits:
        fresh = True
        continue
    text = lits[-1].encode().decode("unicode_escape")
    if fresh and re.fullmatch(r'\s*"(?:[^"\\]|\\.)*"\s*(?:\\|;)?\s*', l) and text.endswith("\n") and len(text.strip()) > 24:
        print(text.strip())
    fresh = text.endswith("\n")
EOF
rsh 'cat /mnt/SDCARD/ZyperOS/diatom' < /dev/null > "$OUT/diatom" || die "could not read the device's Diatom"
strings -a -n 8 "$OUT/diatom" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' > "$OUT/diatom.txt"
missing=0
while IFS= read -r l; do
	grep -qxF -- "$l" "$OUT/diatom.txt" || { missing=$((missing + 1)); echo "shaderdemo-device: not in the device's Diatom: $l" >&2; }
done < "$OUT/glsl.txt"
[ "$missing" = 0 ] || die "the device's Diatom has other GLSL than this tree ($missing lines): deploy this tree first"
echo "shaderdemo-device: the device's Diatom has this tree's GLSL ($(wc -l < "$OUT/glsl.txt" | tr -d ' ') lines checked)"

# The run, as a port: each filter, Diatom 4 s, the panel shot at 3 s
cat > "$OUT/run.sh" <<EOF
#!/bin/sh
D=$D
rm -rf \$D/out; mkdir -p \$D/out \$D/save \$D/sys
for spec in nearest:100 shimmerless:100 crt:100 crt-curved:100 scanlines:100 lcd:100 lcd-unlit:100 crt:50 crt-curved:50 scanlines:50 lcd:50 lcd-unlit:50; do
	f=\${spec%:*}; s=\${spec#*:}; n=\$f; [ "\$s" = 100 ] || n=\$f-\$s
	STUBCORE_IMAGE=\$D/scene.rgb565 /mnt/SDCARD/ZyperOS/diatom --core \$D/stubcore.so --display aspect \\
		--filter \$f --strength \$s --frames 240 --save \$D/save --system \$D/sys > \$D/out/\$n.log 2>&1 &
	p=\$!
	sleep 3
	px2-shot \$D/out/\$n.ppm >> \$D/out/\$n.log 2>&1
	wait \$p
	echo "exit=\$?" >> \$D/out/\$n.log
done
touch \$D/out/done
EOF
rsh "rm -rf $D && mkdir -p $D" < /dev/null || die "no $D on the device"
for f in stubcore.so want/scene.rgb565 run.sh; do
	rsh "cat > $D/$(basename "$f")" < "$OUT/$f" || die "could not copy $f"
done
rsh "printf '#!/bin/sh\nexec timeout 120 sh $D/run.sh\n' > \"$PORT_SCRIPT\" && chmod +x \"$PORT_SCRIPT\"" < /dev/null ||
	die "could not write the port's script"
rsh "px2-open \"$PORT_SCRIPT\"" < /dev/null || die "px2-open refused the port"

# Wait for it, through the drops
i=0
until rsh "test -f $D/out/done" < /dev/null 2> /dev/null; do
	i=$((i + 5))
	[ "$i" -le 180 ] || die "the port did not finish in 3 min"
	sleep 5
done
rsh "cd $D/out && tar cf - ." < /dev/null | tar xf - -C "$OUT/shot" || die "could not fetch the shots"
rsh "rm -f \"$PORT_SCRIPT\"; rm -rf $D" < /dev/null
# The launcher back on its shelf before the lock goes
i=0
until rsh 'px2-open --status' < /dev/null 2> /dev/null | grep -q '^state=shelf'; do
	i=$((i + 3))
	[ "$i" -le 60 ] || { echo "shaderdemo-device: the launcher is not back on the shelf after 60 s" >&2; break; }
	sleep 3
done

# A Python with Pillow (the Makefile's PYIMG, or the first of its candidates)
py=${PYIMG:-}
[ -n "$py" ] || for c in python3 "$HOME/.pyenv/shims/python3" /usr/bin/python3; do
	"$c" -c 'import PIL' 2> /dev/null && py=$c && break
done
[ -n "$py" ] || die "no Python with Pillow to compare with (PYIMG)"
"$py" "$HERE/shaderdemo-compare.py" "$OUT"
