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
# ZYPEROS_DISPLAY (src/launch_screen.c): Sharp Pixels is whole multiples of
# PICO-8's 128 pixels (3x, 384 x 384 on the Pixel 2's 640 x 480), every other
# mode the panel's full height (480 x 480). SDL draws it with the GLES 2
# renderer and turns it onto the Pixel 2's turned panel (launch.sh's
# SDL_RENDER_DRIVER and SDL_KMSDRM_ROTATION).
#
# Leaving it: MENU is ZyperOS's in-game menu over it (portkeys), X there
# Quit; MENU+START held a second goes home even when it hangs (zyhome); START
# is PICO-8's own pause menu. Quit and POWER send TERM, which SDL turns into
# a quit, so PICO-8 saves its settings as it ends: hence exec, below.
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

case ${ZYPEROS_DISPLAY:-} in
integer*|overscale) PIXEL_PERFECT=1 ;;
*)                  PIXEL_PERFECT=0 ;;
esac
# SDL's video driver named, always: with -pixel_perfect 0, pico8_64 (0.2.7)
# sets SDL_VIDEODRIVER to "rpi", the Raspberry Pi's, when nothing has set it
# (its codo_init, setenv without overwrite), and SDL has no such driver here.
# The Pixel 2's SDL is KMS/DRM's; a device.sh that names its own (the Brick's
# pvrfb) keeps it
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"

cd "$HOME_DIR" || exit 1
if [ "$RUN" = -run ]; then
	exec "$P8/pico8_64" -home "$HOME_DIR" -root_path "$ROOT" -desktop "$HOME_DIR/desktop" \
		-windowed 0 -pixel_perfect "$PIXEL_PERFECT" -run "$1"
elif [ -n "$RUN" ]; then
	exec "$P8/pico8_64" -home "$HOME_DIR" -root_path "$ROOT" -desktop "$HOME_DIR/desktop" \
		-windowed 0 -pixel_perfect "$PIXEL_PERFECT" -splore
else
	exec "$P8/pico8_64" -home "$HOME_DIR" -root_path "$ROOT" -desktop "$HOME_DIR/desktop" \
		-windowed 0 -pixel_perfect "$PIXEL_PERFECT"
fi
