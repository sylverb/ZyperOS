/* SPDX-License-Identifier: GPL-3.0-only */
/* The display taken back from Diatom after a game, however the game ended.
 *
 * Diatom drops DRM master before it says PAUSED or EXIT, so after those the
 * launcher's SET_MASTER is answered at once, and the game's last frame is
 * still on the panel's CRTC for the launcher's first page flip to replace.
 *
 * A game that crashes ends another way. Diatom says "EXIT reason=crash" from
 * its signal handler (diatom/src/main.c, on_crash) and dies after: until the
 * kernel closes its DRM file it is still master, and SET_MASTER is refused
 * (EBUSY). The launcher asked once, and with that refusal every page flip SDL
 * queued after it was refused as well (EACCES, SDL's "Could not queue
 * pageflip: -13"), so the shelf never reached the panel until the launcher was
 * started again: LocoRoco in PPSSPP, a SIGSEGV in Mesa's Panfrost driver, on
 * the GKD Pixel 2, 2026-11-13. And when the kernel does close that file, it
 * removes the framebuffers the file made, the one on the panel among them,
 * which leaves the CRTC with nothing on it: a page flip needs a picture to
 * flip from, so SDL's flips would still be refused.
 *
 * So master is asked for until it is free, with a short wait between tries
 * that grows, and a bound on the whole (the launcher asks again at each frame
 * after that). Once master is ours, a CRTC with no picture is given a black
 * one at the panel's mode, in the format of SDL's own frames (a page flip
 * cannot change format), and SDL's next flip replaces it. A clean end costs
 * one ioctl more: the CRTC's state, which says a picture is up.
 *
 * The kernel's own ioctls, with the numbers and structures written out as
 * src/panelgamma.c writes them: the launcher links no libdrm, the structures
 * are the kernel's fixed ABI, and the check (tools/displayback-check.c)
 * builds on a computer without Linux's headers. */
#ifndef DISPLAYBACK_H
#define DISPLAYBACK_H

#include <stdint.h>

/* ioctl(2), the clock and the wait between tries, as these call them; the
 * check puts a stand-in display and a clock of its own here. */
typedef int (*displayback_ioctl_fn)(int fd, unsigned long request, void *arg);
extern displayback_ioctl_fn displayback_ioctl;
extern unsigned (*displayback_now_ms)(void);
extern void (*displayback_sleep_ms)(unsigned ms);

/* The panel as SDL's KMSDRM found it at the start (its connector and the
 * CRTC that shows it), and the black picture once one was made. The picture
 * belongs to the DRM file it was made on: zero this when that file closes. */
typedef struct {
	uint32_t crtc, connector;
	uint32_t fb, handle, fb_w, fb_h, format;
} displayback_panel;

/* The first connected connector and its CRTC, the way SDL picks them: the
 * CRTC its encoder shows, or with none, the first the encoder can use. Needs
 * no master. 0, or -1 with no connected panel. */
int displayback_find(int fd, displayback_panel *p);

/* DRM master, asked for until it is given or max_ms have passed: 1 ms
 * between the first tries, twice that each time after, 50 ms at most. With
 * max_ms 0, one try. Returns the ms it took (0: at once), or -1 with *err the
 * last refusal's errno. */
int displayback_master(int fd, unsigned max_ms, int *err);

/* With master: a picture on the panel's CRTC. 0 when one is up already (a
 * clean end: the game's last frame), 1 when there was none and a black one is
 * up now, at the CRTC's mode or, with the CRTC off, the panel's own, and in
 * the format of SDL's own frames on fd, so SDL's next page flip may replace
 * it; -1 when that failed, errno saying why. */
int displayback_relight(int fd, displayback_panel *p);

#endif
