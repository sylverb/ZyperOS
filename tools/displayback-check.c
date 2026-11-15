/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-displayback: src/displayback.c against a stand-in display and a
 * stand-in clock. The display answers the kernel's DRM ioctls by Linux's own
 * request numbers, as the kernel's headers make them, so a number
 * displayback.c packs wrong is a refusal here as it would be on the Pixel 2.
 * The clock moves only when displayback.c waits.
 *
 * The stand-in plays Diatom as the kernel sees it: master until its DRM file
 * closes, at a time the case picks, and at that close its framebuffer taken
 * off the panel's CRTC (and, when the case says so, the CRTC turned off).
 *
 * The launcher's own DRM file holds SDL's frames, in the format SDL made
 * them in, and the stand-in applies the kernel's page flip rule to SDL's next
 * flip: a picture on the CRTC to flip from, in the same format.
 *
 * The cases: the panel found as SDL finds it; a clean end (master at once,
 * the game's last frame left up, nothing made); a crash (master asked for
 * again until the dying Diatom lets go, a growing wait between tries, then a
 * black picture put up at the CRTC's mode in SDL's format, SDL's next flip
 * allowed, the picture kept for the next crash); a crash that turned the CRTC
 * off (the panel's preferred mode); QEMU's display (XRGB8888 frames; and with
 * no frame to learn from, ARGB8888 refused and XRGB8888 used); a Diatom that
 * never lets go (refused on the bound, EBUSY, a bounded number of tries); the
 * per-frame try (one ask, no wait); no master; no display. */
#include "../src/displayback.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* include/uapi/drm/drm.h's numbers on Linux, as the kernel's headers give
 * them (printed from plastron's SDK, 2026-11-14) */
#define LINUX_SET_MASTER     0x641Eul
#define LINUX_GETRESOURCES   0xC04064A0ul
#define LINUX_GETCRTC        0xC06864A1ul
#define LINUX_SETCRTC        0xC06864A2ul
#define LINUX_GETENCODER     0xC01464A6ul
#define LINUX_GETCONNECTOR   0xC05064A7ul
#define LINUX_RMFB           0xC00464AFul
#define LINUX_CREATE_DUMB    0xC02064B2ul
#define LINUX_DESTROY_DUMB   0xC00464B4ul
#define LINUX_ADDFB2         0xC06864B8ul
#define LINUX_GETFB2         0xC06864CEul
#define ARGB8888             0x34325241u
#define XRGB8888             0x34325258u

struct card_res {
	uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
	uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
	uint32_t min_width, max_width, min_height, max_height;
};
struct modeinfo {
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
	uint32_t vrefresh, flags, type;
	char     name[32];
};
struct mode_crtc {
	uint64_t set_connectors_ptr;
	uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
	struct modeinfo mode;
};
struct get_encoder {
	uint32_t encoder_id, encoder_type, crtc_id, possible_crtcs, possible_clones;
};
struct get_connector {
	uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
	uint32_t count_modes, count_props, count_encoders, encoder_id;
	uint32_t connector_id, connector_type, connector_type_id, connection;
	uint32_t mm_width, mm_height, subpixel, pad;
};
struct fb_cmd2 {
	uint32_t fb_id, width, height, pixel_format, flags;
	uint32_t handles[4], pitches[4], offsets[4];
	uint64_t modifier[4];
};
struct create_dumb {
	uint32_t height, width, bpp, flags, handle, pitch;
	uint64_t size;
};

/* ---- the stand-in clock ---------------------------------------------------- */

static unsigned now, sleeps, slept[64];

static unsigned fake_now(void) { return now; }

static void fake_sleep(unsigned ms)
{
	if (sleeps < sizeof slept / sizeof slept[0]) slept[sleeps] = ms;
	sleeps++;
	now += ms;
}

/* ---- the stand-in display -------------------------------------------------- */

/* The Pixel 2 as plastron's kernel has it: two VOPs (49 the big one, which
 * the panel is on; 67 the little one), the DSI encoder 51 and the panel's
 * connector 52. Connector 60 (and its encoder 59) is listed first and
 * connected to nothing, so the choice is tested. */
enum { CRTC_BIG = 49, CRTC_LIT = 67, ENC_DSI = 51, CONN_DSI = 52, ENC_OTHER = 59, CONN_OTHER = 60 };

static const struct modeinfo PANEL = {
	.clock = 24000, .hdisplay = 480, .hsync_start = 500, .hsync_end = 510, .htotal = 530,
	.vdisplay = 640, .vsync_start = 650, .vsync_end = 652, .vtotal = 660,
	.vrefresh = 60, .type = 1u << 3 | 1u << 6, .name = "480x640",
};
/* A larger mode the panel does not prefer, listed first */
static const struct modeinfo BIGGER = {
	.clock = 74250, .hdisplay = 1280, .hsync_start = 1390, .hsync_end = 1430, .htotal = 1650,
	.vdisplay = 720, .vsync_start = 725, .vsync_end = 730, .vtotal = 750,
	.vrefresh = 60, .type = 1u << 6, .name = "1280x720",
};

#define US 1          /* master: this process */
#define DIATOM 2      /* master: Diatom's file */

static int master;               /* 0 free, US, DIATOM */
static long diatom_closes_at;    /* when Diatom's file closes; -1 never */
static int close_turns_crtc_off; /* the kernel turns the CRTC off at that close */
static int encoder_lit;          /* the DSI encoder shows CRTC_BIG */
static int many_modes;           /* the panel lists 20 modes, more than displayback.c holds */

static struct { uint32_t fb, mode_valid; struct modeinfo mode; } crtc;
#define DIATOM_FB 31             /* the game's last frame */

/* This file's framebuffers: SDL's frames (40 to 42, in sdl_fmt; none when
 * sdl_nfbs is 0) and the black one once made (black_fb, in black_fmt) */
static uint32_t sdl_fmt, sdl_nfbs, black_fb, black_fmt;
static int primary_takes_argb;   /* 0: QEMU's virtio-gpu, whose primary plane refuses it */

static int unknown, set_master_tries, dumbs, fbs, setcrtcs, rmfbs;
static uint32_t dumb_w, dumb_h, last_fb;
static struct fb_cmd2 last_addfb;
static struct mode_crtc last_set;
static uint32_t last_set_conn;

/* Diatom's file closes when the clock has reached the time the case picked:
 * its framebuffers go first, then its master, as drm_file_free does them */
static void diatom_maybe_closes(void)
{
	if (master != DIATOM || diatom_closes_at < 0 || (long)now < diatom_closes_at) return;
	if (crtc.fb == DIATOM_FB) {
		crtc.fb = 0;
		if (close_turns_crtc_off) {
			crtc.mode_valid = 0;
			memset(&crtc.mode, 0, sizeof crtc.mode);
		}
	}
	master = 0;
}

static int fake_ioctl(int fd, unsigned long request, void *arg)
{
	(void)fd;
	diatom_maybe_closes();
	switch (request) {
	case LINUX_SET_MASTER:
		set_master_tries++;
		if (master == DIATOM) { errno = EBUSY; return -1; }
		master = US;
		return 0;
	case LINUX_GETRESOURCES: {
		static const uint32_t crtcs[] = { CRTC_BIG, CRTC_LIT }, conns[] = { CONN_OTHER, CONN_DSI };
		struct card_res *r = arg;
		uint32_t *c = (uint32_t *)(uintptr_t)r->crtc_id_ptr;
		uint32_t *n = (uint32_t *)(uintptr_t)r->connector_id_ptr;

		if (c) for (uint32_t i = 0; i < 2 && i < r->count_crtcs; i++) c[i] = crtcs[i];
		if (n) for (uint32_t i = 0; i < 2 && i < r->count_connectors; i++) n[i] = conns[i];
		uint32_t *f = (uint32_t *)(uintptr_t)r->fb_id_ptr, nf = 0;

		for (uint32_t i = 0; i < sdl_nfbs; i++, nf++)
			if (f && nf < r->count_fbs) f[nf] = 40 + i;
		if (black_fb) {
			if (f && nf < r->count_fbs) f[nf] = black_fb;
			nf++;
		}
		r->count_crtcs = 2;
		r->count_connectors = 2;
		r->count_encoders = 2;
		r->count_fbs = nf;
		return 0;
	}
	case LINUX_GETCONNECTOR: {
		struct get_connector *c = arg;
		uint32_t *e = (uint32_t *)(uintptr_t)c->encoders_ptr;
		struct modeinfo *m = (struct modeinfo *)(uintptr_t)c->modes_ptr;

		if (c->connector_id == CONN_DSI) {
			if (e && c->count_encoders >= 1) e[0] = ENC_DSI;
			/* Whole or not at all, as the kernel copies a list */
			if (m && !many_modes && c->count_modes >= 2) { m[0] = BIGGER; m[1] = PANEL; }
			c->count_encoders = 1;
			c->count_modes = many_modes ? 20 : 2;
			c->encoder_id = encoder_lit ? ENC_DSI : 0;
			c->connection = 1;
		} else if (c->connector_id == CONN_OTHER) {
			if (e && c->count_encoders >= 1) e[0] = ENC_OTHER;
			c->count_encoders = 1;
			c->count_modes = 0;
			c->encoder_id = 0;
			c->connection = 2;           /* disconnected */
		} else {
			errno = ENOENT;
			return -1;
		}
		c->count_props = 0;
		return 0;
	}
	case LINUX_GETENCODER: {
		struct get_encoder *e = arg;

		if (e->encoder_id == ENC_DSI) {
			e->crtc_id = encoder_lit ? CRTC_BIG : 0;
			e->possible_crtcs = 0x3;     /* either VOP; the first is the big one */
		} else if (e->encoder_id == ENC_OTHER) {
			e->crtc_id = 0;
			e->possible_crtcs = 0x2;
		} else {
			errno = ENOENT;
			return -1;
		}
		return 0;
	}
	case LINUX_GETCRTC: {
		struct mode_crtc *c = arg;

		if (c->crtc_id == CRTC_LIT) { c->fb_id = 0; c->mode_valid = 0; return 0; }
		if (c->crtc_id != CRTC_BIG) { errno = ENOENT; return -1; }
		c->fb_id = crtc.fb;
		c->mode_valid = crtc.mode_valid;
		c->mode = crtc.mode;
		return 0;
	}
	case LINUX_CREATE_DUMB: {
		struct create_dumb *d = arg;

		if (d->bpp != 32 || !d->width || !d->height) { errno = EINVAL; return -1; }
		dumbs++;
		dumb_w = d->width;
		dumb_h = d->height;
		d->handle = 7;
		d->pitch = d->width * 4;
		d->size = (uint64_t)d->pitch * d->height;
		return 0;
	}
	case LINUX_ADDFB2: {
		struct fb_cmd2 *f = arg;

		if (f->handles[0] != 7 || f->flags) { errno = ENOENT; return -1; }
		if (f->pixel_format != ARGB8888 && f->pixel_format != XRGB8888) { errno = EINVAL; return -1; }
		fbs++;
		last_addfb = *f;
		f->fb_id = last_fb = black_fb = 90 + (uint32_t)fbs;
		black_fmt = f->pixel_format;
		return 0;
	}
	case LINUX_GETFB2: {
		struct fb_cmd2 *f = arg;

		if (f->fb_id >= 40 && f->fb_id < 40 + sdl_nfbs) f->pixel_format = sdl_fmt;
		else if (f->fb_id && f->fb_id == black_fb) f->pixel_format = black_fmt;
		else { errno = ENOENT; return -1; }
		f->width = 480;
		f->height = 640;
		return 0;
	}
	case LINUX_RMFB:
		if (*(uint32_t *)arg == black_fb) black_fb = 0;
		rmfbs++;
		return 0;
	case LINUX_DESTROY_DUMB:
		return 0;
	case LINUX_SETCRTC: {
		struct mode_crtc *c = arg;

		if (master != US) { errno = EACCES; return -1; }
		if (c->crtc_id != CRTC_BIG || c->count_connectors != 1 || !c->set_connectors_ptr ||
		    !c->mode_valid || c->fb_id != black_fb) {
			errno = EINVAL;
			return -1;
		}
		if (black_fmt == ARGB8888 && !primary_takes_argb) { errno = EINVAL; return -1; }
		setcrtcs++;
		last_set = *c;
		last_set_conn = *(uint32_t *)(uintptr_t)c->set_connectors_ptr;
		crtc.fb = c->fb_id;
		crtc.mode_valid = 1;
		crtc.mode = c->mode;
		return 0;
	}
	default:
		unknown++;
		errno = EINVAL;
		return -1;
	}
}

/* SDL's next page flip, by the kernel's rule (drm_mode_page_flip_ioctl):
 * refused with no picture on the CRTC to flip from (EBUSY), and refused
 * when it would change format (EINVAL). 0 when allowed. */
static int sdl_flip(void)
{
	uint32_t from;

	if (master != US) return EACCES;
	if (!crtc.fb) return EBUSY;
	from = crtc.fb == DIATOM_FB ? sdl_fmt : crtc.fb == black_fb ? black_fmt : 0;
	return from == sdl_fmt ? 0 : EINVAL;
}

/* A game on the panel: Diatom master, its frame on the big VOP at the
 * panel's mode, and its file closing at `closes_at` (-1: never; the clean
 * end has it give master up first, so it is 0 there) */
static void game_up(long closes_at, int turns_off)
{
	master = DIATOM;
	diatom_closes_at = closes_at;
	close_turns_crtc_off = turns_off;
	encoder_lit = 1;
	many_modes = 0;
	sdl_fmt = ARGB8888;              /* the Pixel 2's, as Diatom measured */
	sdl_nfbs = 3;
	primary_takes_argb = 1;
	crtc.fb = DIATOM_FB;
	crtc.mode_valid = 1;
	crtc.mode = PANEL;
	now = 1000;
	sleeps = 0;
	memset(slept, 0, sizeof slept);
	unknown = set_master_tries = setcrtcs = rmfbs = 0;
	memset(&last_set, 0, sizeof last_set);
	last_set_conn = 0;
}

static int fails;

#define CHECK(cond, what) do { \
	if (cond) printf("  ok   %s\n", what); \
	else { printf("  FAIL %s\n", what); fails++; } \
} while (0)

static int same_mode(const struct modeinfo *a, const struct modeinfo *b)
{
	return !memcmp(a, b, sizeof *a);
}

int main(void)
{
	displayback_panel p;
	int waited, err, r, grew;

	printf("check-displayback:\n");
	displayback_ioctl = fake_ioctl;
	displayback_now_ms = fake_now;
	displayback_sleep_ms = fake_sleep;

	/* The panel, as SDL found it at the start */
	game_up(-1, 0);
	master = 0;
	r = displayback_find(3, &p);
	CHECK(r == 0 && p.connector == CONN_DSI && p.crtc == CRTC_BIG,
	      "the panel: the connected connector (past one connected to nothing) and the CRTC it shows");
	CHECK(p.fb == 0, "  ...and no picture made for it yet");
	encoder_lit = 0;
	r = displayback_find(3, &p);
	CHECK(r == 0 && p.connector == CONN_DSI && p.crtc == CRTC_BIG,
	      "with nothing lit: the first CRTC its encoder can use, as SDL picks it");
	encoder_lit = 1;
	CHECK(unknown == 0, "  ...every request a number Linux knows");

	/* A clean end: Diatom gave master up before EXIT */
	displayback_find(3, &p);
	game_up(0, 0);
	master = 0;
	err = 0;
	waited = displayback_master(3, 1000, &err);
	CHECK(waited == 0 && master == US && sleeps == 0 && set_master_tries == 1,
	      "a clean end: master at the first ask, no wait");
	r = displayback_relight(3, &p);
	CHECK(r == 0 && setcrtcs == 0 && p.fb == 0 && crtc.fb == DIATOM_FB,
	      "  ...the game's last frame left up for the shelf's first flip; nothing made, nothing set");

	/* A crash: EXIT came from Diatom's signal handler, and its file closes
	 * 37 ms later, taking its frame off the CRTC */
	game_up(1037, 0);
	err = 0;
	waited = displayback_master(3, 1000, &err);
	grew = sleeps >= 6 && slept[0] == 1 && slept[1] == 2 && slept[2] == 4 && slept[3] == 8 &&
	       slept[4] == 16 && slept[5] == 32;
	CHECK(waited >= 37 && waited < 37 + 50 && master == US,
	      "a crash: master asked for again until the dying Diatom lets go (37 ms), and given");
	CHECK(grew, "  ...the wait between tries growing from 1 ms, twice each time");
	r = displayback_relight(3, &p);
	CHECK(r == 1 && setcrtcs == 1 && dumbs == 1 && fbs == 1,
	      "  ...nothing on the panel then: one black picture made and put up (1)");
	CHECK(dumb_w == 480 && dumb_h == 640 && last_addfb.width == 480 && last_addfb.height == 640 &&
	      last_addfb.pitches[0] == 480 * 4 && last_addfb.pixel_format == ARGB8888,
	      "  ...at the panel's size, in the format of SDL's own frames (ARGB8888 on the Pixel 2)");
	CHECK(sdl_flip() == 0, "  ...so SDL's next page flip is allowed (a picture to flip from, the same format)");
	CHECK(last_set.crtc_id == CRTC_BIG && last_set_conn == CONN_DSI && last_set.count_connectors == 1 &&
	      same_mode(&last_set.mode, &PANEL) && crtc.fb == p.fb,
	      "  ...on the panel's CRTC and connector, at the mode the CRTC still had");
	r = displayback_relight(3, &p);
	CHECK(r == 0 && setcrtcs == 1, "  ...asked again with it up: nothing more to do");
	CHECK(unknown == 0, "  ...every request a number Linux knows");

	/* The next crash: the same picture again, none made */
	game_up(1005, 0);
	displayback_master(3, 1000, &err);
	r = displayback_relight(3, &p);
	CHECK(r == 1 && dumbs == 1 && fbs == 1 && setcrtcs == 1 && crtc.fb == p.fb,
	      "the next crash: the black picture made the first time, put up again");
	CHECK(sdl_flip() == 0, "  ...and SDL's flip allowed after it");

	/* A crash after which the kernel turned the CRTC off as well */
	game_up(1020, 1);
	displayback_master(3, 1000, &err);
	r = displayback_relight(3, &p);
	CHECK(r == 1 && setcrtcs == 1 && same_mode(&last_set.mode, &PANEL) && crtc.mode_valid,
	      "a crash that turned the CRTC off: lit again at the panel's preferred mode, not its largest");

	/* A connector with more modes than displayback.c holds: the kernel copies
	 * none of them, and none is read */
	game_up(1020, 1);
	many_modes = 1;
	displayback_master(3, 1000, &err);
	r = displayback_relight(3, &p);
	CHECK(r == -1 && setcrtcs == 0 && !crtc.mode_valid,
	      "a panel listing more modes than it holds, the CRTC off: refused (-1), no mode made up");
	many_modes = 0;

	/* QEMU's display: SDL's frames are XRGB8888, its primary plane taking
	 * no ARGB8888. The picture made in ARGB8888 for the Pixel 2 above is
	 * made again in SDL's format. */
	game_up(1010, 0);
	sdl_fmt = XRGB8888;
	primary_takes_argb = 0;
	displayback_master(3, 1000, &err);
	r = displayback_relight(3, &p);
	CHECK(r == 1 && dumbs == 2 && last_addfb.pixel_format == XRGB8888 && rmfbs == 1 && sdl_flip() == 0,
	      "QEMU's display: the black picture made again in SDL's XRGB8888, the old one let go, SDL's flip allowed");

	/* No frame of SDL's to learn the format from, on QEMU's display */
	game_up(1010, 0);
	sdl_fmt = XRGB8888;
	sdl_nfbs = 0;
	primary_takes_argb = 0;
	memset(&p, 0, sizeof p);
	black_fb = 0;
	displayback_find(3, &p);
	displayback_master(3, 1000, &err);
	r = displayback_relight(3, &p);
	CHECK(r == 1 && p.format == XRGB8888 && crtc.fb == p.fb,
	      "no frame of SDL's to read: ARGB8888 tried, refused, XRGB8888 put up");
	CHECK(unknown == 0, "  ...every request a number Linux knows");

	/* The old way, for the record: one ask while Diatom still holds master,
	 * and SDL's flips refused from then on */
	game_up(1037, 0);
	waited = displayback_master(3, 0, &err);
	CHECK(waited == -1 && sdl_flip() == EACCES,
	      "asked only once at a crash (as it first was): refused, and every flip after it refused (EACCES)");

	/* A Diatom that never lets go */
	game_up(-1, 0);
	err = 0;
	waited = displayback_master(3, 1000, &err);
	CHECK(waited == -1 && err == EBUSY && master == DIATOM,
	      "master never given: refused, with the kernel's EBUSY");
	CHECK(now - 1000 == 1000, "  ...on the bound itself, not past it");
	CHECK(set_master_tries <= 30, "  ...after a bounded number of tries (at most 50 ms apart)");

	/* The per-frame try, once the bound has passed */
	game_up(-1, 0);
	err = 0;
	waited = displayback_master(3, 0, &err);
	CHECK(waited == -1 && set_master_tries == 1 && sleeps == 0 && err == EBUSY,
	      "the try at each frame: one ask, no wait");
	diatom_closes_at = 1000;
	waited = displayback_master(3, 0, &err);
	CHECK(waited == 0 && master == US, "  ...and given on the frame after Diatom's file closed");

	/* Without master the kernel refuses the picture */
	game_up(1010, 0);
	now = 1020;
	diatom_maybe_closes();
	r = displayback_relight(3, &p);
	CHECK(r == -1 && errno == EACCES && crtc.fb == 0, "without master: refused (-1, EACCES), nothing up");

	/* No display */
	r = displayback_find(-1, &p);
	CHECK(r == -1 && p.crtc == 0 && p.connector == 0, "no display: no panel found");
	r = displayback_relight(-1, &p);
	CHECK(r == -1, "  ...and nothing to light");

	printf(fails ? "check-displayback: %d FAILED\n" : "check-displayback: all passed\n", fails);
	return fails != 0;
}
