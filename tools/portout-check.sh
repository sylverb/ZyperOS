#!/bin/sh
# A port's latest output on its loading screen (DESIGN.md 11.11).
#
#     make check-portout
#     sh tools/portout-check.sh --shots DIR    and the screen as the lines change
#
# While a port starts, plastron's splash holds the loading screen the launcher
# saved and draws the latest line the port prints at its foot
# (plastron/buildroot/package/splash/src/portout.h), with letters and a box
# the launcher writes beside the screen (src/portload.c, port_out_write).
#
# First the reader and the drawing on their own, with letters made in the
# check (build-native/portout-check): a carriage return, "\r\n", backspace,
# tabs, escapes, UTF-8 and ZyperOS's own lines read as a terminal shows them;
# the box restored before each line, a line too wide cut at a whole letter
# with an ellipsis inside it, and the box turned as the Pixel 2's panel is.
#
# Then the letters the launcher writes, in English and German, Beard and Salt
# & Pepper, shot with the host build (--portload with ZYPEROS_PORTLOAD_OUT):
# each reads, sits on the foot of the panel's 640x480 under Zyper's frame, and
# has every printable ASCII letter. Skipped cleanly without SDL2, as
# check-portload is.
#
# --shots DIR: each of those screens as splash shows it while a stand-in port
# (tools/portout-standin.sh) prints, the screen saved as the device
# saves it (480x640, turned) and followed every 250 ms by splash's own code
# (portout-check --follow), each change of the line a PNG, and one sheet of
# them all, DIR/sheet.png. It needs Pillow.
set -u

D=build-native/portout-check.d
BIN=build-native/zyperos
CHECK=build-native/portout-check
SHOTS=
[ "${1:-}" = --shots ] && SHOTS=${2:?--shots needs a folder}

"$CHECK" || exit 1

if ! pkg-config --exists sdl2 SDL2_image SDL2_ttf 2>/dev/null; then
	echo "skipped the launcher's letters: no SDL2 here to build the launcher with"
	exit 0
fi
if [ ! -x "$BIN" ]; then
	echo "FAIL no $BIN: make native first" >&2
	exit 1
fi

rm -rf "$D"
mkdir -p "$D/root" "$D/Roms/Ports" "$D/userdata" "$D/shared" || exit 1
cp config/systems.cfg "$D/root/systems.cfg" || exit 1
printf '#!/bin/sh\n' >"$D/Roms/Ports/Star Farm.sh" || exit 1

fails=0
echo "the launcher's letters for splash, in English and German, light and dark:"
for lang in en de; do
	for theme in light dark; do
		for later in "" --portload-later; do
			name=$lang-$theme${later:+-later}
			ZYPEROS_HEADLESS=1 SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
			ZYPEROS_ROOT="$D/root" ZYPEROS_CARD="$D" ZYPEROS_ROMS="$D/Roms" \
			ZYPEROS_USERDATA="$D/userdata" ZYPEROS_SHARED="$D/shared" ZYPEROS_WEB="$D" \
			ZYPEROS_FONT=res/fonts/menu.ttf ZYPEROS_PORTLOAD_OUT="$D/$name.out" \
				"$BIN" --shot "$D/$name.png" --theme "$theme" --lang "$lang" $later \
				--portload "Star Farm" >"$D/$name.log" 2>&1
			st=$?
			if [ "$st" -ne 0 ] || [ ! -s "$D/$name.png" ]; then
				echo "  FAIL $name: the shot failed, exit $st (see $D/$name.log)"
				grep "short\|could not\|Zyper" "$D/$name.log" | sed 's/^/    /'
				fails=$((fails + 1))
				continue
			fi
			"$CHECK" --atlas "$D/$name.out" || fails=$((fails + 1))
		done
	done
done
# The first shot drew the letters and kept them on the card (.userdata's
# portout-letters.bin); the others read them back. The same letters either way
if ! grep -q "drawn and kept" "$D/en-light.log" || ! grep -q "the kept ones" "$D/de-dark.log"; then
	echo "  FAIL the first shot did not draw and keep the letters, or a later one did not read them back"
	fails=$((fails + 1))
elif ! { tail -c +25 "$D/en-light.out" >"$D/drawn.letters" &&
          tail -c +25 "$D/de-dark.out" >"$D/kept.letters" &&
          cmp -s "$D/drawn.letters" "$D/kept.letters"; }; then
	echo "  FAIL the letters read back are not the ones drawn"
	fails=$((fails + 1))
fi
if [ "$fails" -ne 0 ]; then
	printf '\n%d FAILED\n' "$fails"
	exit 1
fi
echo "ok: the launcher's letters read, on the foot under Zyper's frame, every ASCII letter in them,"
echo "drawn once and read back the same"
[ -n "$SHOTS" ] || exit 0

# The screens as splash shows them while the stand-in prints
PY=${PYIMG:-python3}
mkdir -p "$SHOTS" || exit 1
for name in en-light de-light en-dark de-dark en-light-later de-dark-later; do
	"$PY" - "$D/$name.png" "$D/$name.raw" <<'EOF' || exit 1
import sys
from PIL import Image
# As the Pixel 2 saves it (src/device/pixel2.c, plat_save_screen): the
# 1024x768 screen at the panel's 640x480, turned onto its 480x640: a
# landscape pixel (x, y) is the panel's row 639 - x, column y
im = Image.open(sys.argv[1]).convert("RGB").resize((640, 480), Image.BOX)
turned = im.transpose(Image.Transpose.ROTATE_90)
assert turned.size == (480, 640)
with open(sys.argv[2], "wb") as f:
    for r, g, b in turned.getdata():
        f.write(bytes((b, g, r, 255)))
EOF
	: >"$D/$name.port.log"
	"$CHECK" --follow "$D/$name.out" "$D/$name.raw" "$D/$name.port.log" "$SHOTS/$name" 9 \
		>"$SHOTS/$name.txt" &
	follow=$!
	sleep 0.4
	PORTOUT_STEP=0.5 sh tools/portout-standin.sh >>"$D/$name.port.log" 2>&1
	wait "$follow"
	echo "  $name: $(grep -c ppm "$SHOTS/$name.txt") frames"
done
"$PY" - "$SHOTS" <<'EOF'
import glob, os, sys
from PIL import Image, ImageDraw, ImageFont
out = sys.argv[1]
for ppm in sorted(glob.glob(os.path.join(out, "*.ppm"))):
    Image.open(ppm).save(ppm[:-4] + ".png")
    os.remove(ppm)
rows = ["en-light", "de-light", "en-dark", "de-dark", "en-light-later", "de-dark-later"]
cols = []
for r in rows:
    frames = sorted(glob.glob(os.path.join(out, r + "-[0-9][0-9].png")))
    # the first (nothing yet), the progress, the long line cut, the last
    pick = [frames[0], frames[4], frames[-3], frames[-1]] if len(frames) >= 6 else frames
    cols.append((r, pick))
w, h, gap, strip = 640, 480, 12, 24
n = max(len(p) for _, p in cols)
sheet = Image.new("RGB", (n * (w + gap) - gap, len(cols) * (h + strip + gap) - gap), (128, 128, 128))
d = ImageDraw.Draw(sheet)
try:
    f = ImageFont.truetype("res/fonts/menu.ttf", 16)
except OSError:
    f = None
for j, (r, pick) in enumerate(cols):
    y = j * (h + strip + gap)
    for i, p in enumerate(pick):
        sheet.paste(Image.open(p), (i * (w + gap), y + strip))
        d.text((i * (w + gap) + 4, y + 4), "%s  %s" % (r, os.path.basename(p)[len(r) + 1:-4]),
               font=f, fill=(255, 255, 255))
sheet.save(os.path.join(out, "sheet.png"))
print("  sheet: %s" % os.path.join(out, "sheet.png"))
EOF
