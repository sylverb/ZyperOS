#!/bin/sh
# Your PICO-8: the PICO-8 shelf's first player, Lexaloffle's own PICO-8, the
# player's copy (config/systems.cfg's PICO-8 row and its brings line,
# src/pico8.h). launch.sh's run_port starts this with the cart as its
# argument, or with Splore's entry (Splore.splore), for Splore. POSIX sh, so
# a handheld whose image has no bash runs it too.
#
# ZyperOS never ships, downloads or keeps PICO-8: it is Lexaloffle's to sell.
# The player brings the PICO-8 for Raspberry Pi zip they bought. ZyperOS
# Installer copies its pico8_64 (the 64-bit ARM program) and pico8.dat (its
# data, which it looks for beside itself) into Bios/PICO-8, or the player
# copies them, or the zip, there by hand (the launcher takes the files out of
# a zip there: src/pico8.c).
#
# Where it keeps things, with -home: its settings (config.txt, which it writes
# on its way out), Splore's carts (bbs/), favourites and its log, in
# Saves/PICO-8. A cart's own saves (cartdata) are one set for both players
# (Tarik's pick, 2026-11-19): fake08's Saves/cdata, bound over its cdata/ for
# the run (launch.sh's port_unmount takes the bind off after). Its root, where
# its own file list looks, is the PICO-8 shelf's folder.
#
# The picture: the shelf's Display row, as launch.sh hands it over in
# ZYPEROS_DISPLAY (src/launch_screen.c), or the in-game menu's Display row,
# which starts this again with the new one (launch.sh's port_quit). Each mode
# is where Diatom puts a 128 x 128 picture in it, so the row means over your
# PICO-8 what it means over fake08 (src/p8draw.h has the table): PICO-8's own
# -draw_rect, the screen's place and size in its window, for every mode, and
# -pixel_perfect 1, unfiltered, where the size is a whole multiple of 128.
# SDL draws it with the GLES 2 renderer and turns it onto the Pixel 2's
# turned panel (launch.sh's SDL_RENDER_DRIVER and SDL_KMSDRM_ROTATION).
#
# Leaving it: MENU is ZyperOS's in-game menu over it (portkeys), X there
# Quit; MENU+START held a second goes home even when it hangs (zyhome); START
# is PICO-8's own pause menu. Quit and POWER send TERM, which SDL turns into
# a quit, so PICO-8 saves its settings as it ends: hence exec, below.
#
# MP4 plays on over it, as over a Diatom game, and SELECT in that menu opens
# it (DESIGN.md 11.55): launch.sh leaves MP4's daemon running for this
# script alone and starts its follower beside it (src/mp4hand.h). PICO-8's
# sound goes to its ALSA default, through the equalizer's plugin into the
# same dmix MP4 plays into (sd/zyperos/eq-port.sh), so the two mix.
CARD=/mnt/SDCARD
P8="$CARD/Bios/PICO-8"
HOME_DIR="$CARD/Saves/PICO-8"
CDATA="$CARD/Saves/cdata"

if [ ! -f "$P8/pico8_64" ] || [ ! -f "$P8/pico8.dat" ]; then
	echo "pico8: Bios/PICO-8 needs pico8_64 and pico8.dat from the PICO-8 for Raspberry Pi zip"
	exit 1
fi
# A copy by hand onto a card whose file system keeps no permissions can come
# without the program's: put back where it can be
[ -x "$P8/pico8_64" ] || chmod +x "$P8/pico8_64" 2> /dev/null

mkdir -p "$HOME_DIR/cdata" "$HOME_DIR/desktop" "$CDATA"
# One set of cart saves with fake08's: Saves/cdata seen as PICO-8's own
if ! grep -q " $HOME_DIR/cdata " /proc/mounts 2> /dev/null; then
	mount --bind "$CDATA" "$HOME_DIR/cdata" ||
		echo "pico8: could not bind Saves/cdata; its cart saves stay in Saves/PICO-8/cdata"
fi

VERSION=
[ -f "$P8/version.txt" ] && read -r VERSION < "$P8/version.txt"

# The cart's folder is PICO-8's root; Splore's, the shelf's
case $1 in
*.splore) ROOT=${1%/*}; RUN=-splore; echo "pico8: PICO-8 ${VERSION:-(no version.txt)}, Splore" ;;
"")       ROOT="$CARD/Roms/PICO-8"; RUN=; echo "pico8: PICO-8 ${VERSION:-(no version.txt)}, its own start" ;;
*)        ROOT=${1%/*}; RUN=-run; echo "pico8: PICO-8 ${VERSION:-(no version.txt)}, $1" ;;
esac
[ -d "$ROOT" ] || ROOT="$CARD/Roms/PICO-8"

# 1 while pico8_64 takes a -draw_rect that starts above or left of the
# panel: Sharp Pixels Zoomed and Fill Edges Cut do (y -80 on the Pixel 2).
# 0.2.7 hands the x and y to SDL as they are on its GPU path, by a reading of
# the program, but no device has shown it. If one shows PICO-8 refusing it,
# 0 turns those two into the nearest mode on the panel, Zoomed into Sharp
# Pixels and Edges Cut into Original Shape: src/p8draw.h's P8_NEG_OK goes with
# it, and make check-pico8 holds the two the same.
NEG_OK=1

# The window PICO-8 gets: the panel's first mode, landscape, as SDL turns a
# portrait panel (launch.sh); 640 x 480 when the system does not say
W=640 H=480
for m in /sys/class/drm/card*-*/modes; do
	read -r m < "$m" 2> /dev/null || continue
	case $m in
	[0-9]*x[0-9]*) W=${m%%x*} H=${m#*x} H=${H%%[!0-9]*}; break ;;
	esac
done
[ "$H" -gt "$W" ] && { m=$W; W=$H; H=$m; }

# The mode drawn, and its size: Diatom's arithmetic for a square 128-pixel
# picture (src/dmodefit.c). None, or a word it does not know, is Original
# Shape, the 480 x 480 it drew before its Display modes came: the launcher
# hands that for a shelf whose Display was never set (src/main.c's
# dmode_default), and only a pick of Full Screen stretches it
MODE=${ZYPEROS_DISPLAY:-aspect}
if [ "$NEG_OK" = 0 ]; then
	case $MODE in overscale) MODE=integer ;; fill) MODE=aspect ;; esac
fi
SMALL=$H BIG=$W
[ "$W" -lt "$H" ] && { SMALL=$W; BIG=$H; }
case $MODE in
native)
	DW=128 DH=128 ;;
integer|integer-vertical)
	F=$((SMALL / 128)); [ "$F" -lt 1 ] && F=1
	DW=$((128 * F)) DH=$DW ;;
overscale)
	F=$(((BIG + 127) / 128)); [ "$F" -lt 1 ] && F=1
	DW=$((128 * F)) DH=$DW ;;
aspect|square)
	DW=$SMALL DH=$SMALL ;;
fill)
	DW=$BIG DH=$BIG ;;
stretch)
	DW=$W DH=$H ;;
*)
	MODE=aspect DW=$SMALL DH=$SMALL ;;
esac
DRAW_RECT=$(((W - DW) / 2)),$(((H - DH) / 2)),$DW,$DH
if [ $((DW % 128)) = 0 ] && [ $((DH % 128)) = 0 ]; then PIXEL_PERFECT=1; else PIXEL_PERFECT=0; fi
echo "pico8: Display ${ZYPEROS_DISPLAY:-(none)}, drawn $MODE at $DRAW_RECT on ${W}x${H}"
# SDL's video driver named, always: with -pixel_perfect 0, pico8_64 (0.2.7)
# sets SDL_VIDEODRIVER to "rpi", the Raspberry Pi's, when nothing has set it
# (its codo_init, setenv without overwrite), and SDL has no such driver here.
# The Pixel 2's SDL is KMS/DRM's; a device.sh that names its own (the Brick's
# pvrfb) keeps it
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
# O at the bottom and X beside it, as fake08 lays PICO-8 out (src/controls.c)
# and the Controls pages say. PICO-8 takes SDL's "a" as O, and SDL's own
# mapping of the Pixel 2's joypad (its log.txt) has "a" on BTN_SOUTH, which
# this pad sends from the A cap, on the right: so O was the A cap and X the B
# cap (device test, 2026-11-19). The same mapping with the two swapped, for
# this pad's id, which SDL reads before any mapping of PICO-8's own
export SDL_GAMECONTROLLERCONFIG="190043d2010000000221000000010000,gkd_pixel2_joypad,a:b1,b:b0,x:b2,y:b3,back:b8,start:b9,leftshoulder:b4,rightshoulder:b5,dpup:b10,dpdown:b11,dpleft:b12,dpright:b13,lefttrigger:b6,righttrigger:b7,crc:d243,platform:Linux,"
# Splore's downloads: pico8_64 runs `wget` for every list and cart, and the
# system's wget (busybox's) has no HTTPS, which every cart is. Ours, in bin/
# beside this script, hands them to curl (PICO-8-wget.sh says how), so it
# goes first on PICO-8's PATH
case $0 in */*) HERE=$(cd "${0%/*}" 2> /dev/null && pwd) ;; *) HERE=$(pwd) ;; esac
if [ -n "$HERE" ] && [ -x "$HERE/bin/wget" ]; then
	export PATH="$HERE/bin:$PATH"
else
	echo "pico8: no bin/wget beside this script; Splore downloads with the system's wget, which has no HTTPS"
fi

cd "$HOME_DIR" || exit 1
if [ "$RUN" = -run ]; then
	exec "$P8/pico8_64" -home "$HOME_DIR" -root_path "$ROOT" -desktop "$HOME_DIR/desktop" \
		-windowed 0 -pixel_perfect "$PIXEL_PERFECT" -draw_rect "$DRAW_RECT" -run "$1"
elif [ -n "$RUN" ]; then
	exec "$P8/pico8_64" -home "$HOME_DIR" -root_path "$ROOT" -desktop "$HOME_DIR/desktop" \
		-windowed 0 -pixel_perfect "$PIXEL_PERFECT" -draw_rect "$DRAW_RECT" -splore
else
	exec "$P8/pico8_64" -home "$HOME_DIR" -root_path "$ROOT" -desktop "$HOME_DIR/desktop" \
		-windowed 0 -pixel_perfect "$PIXEL_PERFECT" -draw_rect "$DRAW_RECT"
fi
