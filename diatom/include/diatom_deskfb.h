/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef DIATOM_DESKFB_H
#define DIATOM_DESKFB_H

/* The desk frame file: the desktop port's frames, and the buttons it plays
 * with, shared with ZyperOS's launcher in the host's desk mode (ZyperOS's
 * src/device/desk.h) through a file both map, so a game runs in the
 * launcher's own window, under one keyboard focus, with its in-game menu over
 * it, as on a handheld whose launcher and emulator take turns on one panel.
 *
 * DIATOM_DESKFB=path: the port draws into a hidden window with SDL's software
 * renderer, at DIATOM_SURFACE (the handheld's panel), and after each frame
 * copies what it drew into the file, under a sequence number: odd while the
 * copy runs, even when the frame is whole (a seqlock). The launcher writes
 * `buttons`, the DIATOM_BIT()s it holds, which the port reads at each poll as
 * a keyboard's, and reads `polled`, the buttons the last whole frame was made
 * with, to time a press to the frame it changed.
 *
 * The pixels follow the header: w * h, 4 bytes each, B G R X in memory
 * (SDL's ARGB8888 on a little-endian computer). */

#include <stdint.h>

#define DIATOM_DESKFB_MAGIC   0x46445a44u   /* "DZDF" */
#define DIATOM_DESKFB_VERSION 1u

typedef struct {
	uint32_t magic, version;
	uint32_t w, h;              /* the surface: the panel's pixels */
	uint32_t seq;               /* odd while a frame is being copied in */
	uint32_t buttons;           /* the launcher's: DIATOM_BIT()s held */
	uint32_t polled;            /* the port's: the buttons the frame used */
	uint32_t frames;            /* frames copied in since the port started */
	uint32_t reserved[8];
} diatom_deskfb;

#define DIATOM_DESKFB_BYTES(w, h) (sizeof(diatom_deskfb) + (size_t)(w) * (size_t)(h) * 4u)

#endif
