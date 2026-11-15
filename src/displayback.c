/* SPDX-License-Identifier: GPL-3.0-only */
/* The display taken back from Diatom after a game. See src/displayback.h. */
#include "displayback.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>

/* Linux's _IO('d', nr) and _IOWR('d', nr, type): DRM's letter, and for the
 * second read and write with the structure's size. Written out because a
 * Mac's macros pack them differently. */
#define DB_IO(nr) ((unsigned long)('d' << 8 | (nr)))
#define DB_IOWR(nr, size) \
	((unsigned long)(3u << 30 | (unsigned)(size) << 16 | 'd' << 8 | (nr)))

/* From the kernel's include/uapi/drm/drm_mode.h, field for field; the sizes
 * checked against plastron's SDK headers, 2026-11-14 */
struct db_card_res {                   /* DRM_IOCTL_MODE_GETRESOURCES */
	uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
	uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
	uint32_t min_width, max_width, min_height, max_height;
};
struct db_modeinfo {                   /* struct drm_mode_modeinfo */
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
	uint32_t vrefresh, flags, type;
	char     name[32];
};
struct db_crtc {                       /* DRM_IOCTL_MODE_GETCRTC, SETCRTC */
	uint64_t set_connectors_ptr;
	uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
	struct db_modeinfo mode;
};
struct db_encoder {                    /* DRM_IOCTL_MODE_GETENCODER */
	uint32_t encoder_id, encoder_type, crtc_id, possible_crtcs, possible_clones;
};
struct db_connector {                  /* DRM_IOCTL_MODE_GETCONNECTOR */
	uint64_t encoders_ptr, modes_ptr, props_ptr, prop_values_ptr;
	uint32_t count_modes, count_props, count_encoders, encoder_id;
	uint32_t connector_id, connector_type, connector_type_id, connection;
	uint32_t mm_width, mm_height, subpixel, pad;
};
struct db_fb_cmd2 {                    /* DRM_IOCTL_MODE_ADDFB2, GETFB2 */
	uint32_t fb_id, width, height, pixel_format, flags;
	uint32_t handles[4], pitches[4], offsets[4];
	uint64_t modifier[4];
};
struct db_create_dumb {                /* DRM_IOCTL_MODE_CREATE_DUMB */
	uint32_t height, width, bpp, flags, handle, pitch;
	uint64_t size;
};
_Static_assert(sizeof(struct db_card_res) == 64, "drm_mode_card_res");
_Static_assert(sizeof(struct db_modeinfo) == 68, "drm_mode_modeinfo");
_Static_assert(sizeof(struct db_crtc) == 104, "drm_mode_crtc");
_Static_assert(offsetof(struct db_crtc, mode) == 36, "drm_mode_crtc.mode");
_Static_assert(sizeof(struct db_encoder) == 20, "drm_mode_get_encoder");
_Static_assert(sizeof(struct db_connector) == 80, "drm_mode_get_connector");
_Static_assert(offsetof(struct db_connector, connection) == 60, "drm_mode_get_connector.connection");
_Static_assert(sizeof(struct db_fb_cmd2) == 104, "drm_mode_fb_cmd2");
_Static_assert(offsetof(struct db_fb_cmd2, modifier) == 72, "drm_mode_fb_cmd2.modifier");
_Static_assert(sizeof(struct db_create_dumb) == 32, "drm_mode_create_dumb");

#define DB_SET_MASTER    DB_IO(0x1e)
#define DB_GETRESOURCES  DB_IOWR(0xA0, sizeof(struct db_card_res))
#define DB_GETCRTC       DB_IOWR(0xA1, sizeof(struct db_crtc))
#define DB_SETCRTC       DB_IOWR(0xA2, sizeof(struct db_crtc))
#define DB_GETENCODER    DB_IOWR(0xA6, sizeof(struct db_encoder))
#define DB_GETCONNECTOR  DB_IOWR(0xA7, sizeof(struct db_connector))
#define DB_RMFB          DB_IOWR(0xAF, sizeof(uint32_t))
#define DB_CREATE_DUMB   DB_IOWR(0xB2, sizeof(struct db_create_dumb))
#define DB_DESTROY_DUMB  DB_IOWR(0xB4, sizeof(uint32_t))
#define DB_ADDFB2        DB_IOWR(0xB8, sizeof(struct db_fb_cmd2))
#define DB_GETFB2        DB_IOWR(0xCE, sizeof(struct db_fb_cmd2))

/* include/uapi/drm/drm_fourcc.h */
#define DB_ARGB8888      0x34325241u   /* 'AR24' */
#define DB_XRGB8888      0x34325258u   /* 'XR24' */

#define DB_CONNECTED     1u            /* drm_mode_get_connector.connection */
#define DB_MODE_PREFERRED (1u << 3)    /* DRM_MODE_TYPE_PREFERRED */

#define DB_MAX 8                       /* CRTCs, connectors, encoders; the Pixel 2 has 2, 1, 1 */
#define DB_MAX_MODES 16                /* a connector's; the Pixel 2's panel has 1 */
#define DB_MAX_FBS 16                  /* this file's framebuffers; SDL makes 3 or so */
#define DB_STEP_MAX 50                 /* ms between tries for master, at most */

static int sys_ioctl(int fd, unsigned long request, void *arg)
{
	return ioctl(fd, request, arg);
}

static unsigned sys_now_ms(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (unsigned)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

static void sys_sleep_ms(unsigned ms)
{
	struct timespec t = { (time_t)(ms / 1000), (long)(ms % 1000) * 1000000L };

	while (nanosleep(&t, &t) != 0 && errno == EINTR) { }
}

displayback_ioctl_fn displayback_ioctl = sys_ioctl;
unsigned (*displayback_now_ms)(void) = sys_now_ms;
void (*displayback_sleep_ms)(unsigned ms) = sys_sleep_ms;

int displayback_master(int fd, unsigned max_ms, int *err)
{
	unsigned t0 = displayback_now_ms(), step = 1, waited;

	for (;;) {
		if (displayback_ioctl(fd, DB_SET_MASTER, NULL) == 0)
			return (int)(displayback_now_ms() - t0);
		if (err) *err = errno;
		waited = displayback_now_ms() - t0;
		if (waited >= max_ms) return -1;
		/* The last wait ends on the bound, for one last try there */
		if (step > max_ms - waited) step = max_ms - waited;
		displayback_sleep_ms(step);
		step = step * 2 > DB_STEP_MAX ? DB_STEP_MAX : step * 2;
	}
}

/* The connector's encoders (DB_MAX at most), and its modes too when `modes`
 * (DB_MAX_MODES): its counts, then its lists. The kernel copies each list
 * whole or not at all, so a list longer than was offered - longer than these
 * hold, or grown between the two asks - comes back empty here. */
static int get_connector(int fd, uint32_t id, struct db_connector *c,
                         uint32_t *encs, struct db_modeinfo *modes)
{
	uint32_t ne, nm;

	memset(c, 0, sizeof *c);
	c->connector_id = id;
	if (displayback_ioctl(fd, DB_GETCONNECTOR, c) != 0) return -1;
	ne = c->count_encoders < DB_MAX ? c->count_encoders : DB_MAX;
	nm = !modes ? 0 : c->count_modes < DB_MAX_MODES ? c->count_modes : DB_MAX_MODES;
	c->count_encoders = ne;
	c->count_modes = nm;
	c->count_props = 0;
	c->encoders_ptr = (uint64_t)(uintptr_t)encs;
	c->modes_ptr = (uint64_t)(uintptr_t)modes;
	c->props_ptr = c->prop_values_ptr = 0;
	if (displayback_ioctl(fd, DB_GETCONNECTOR, c) != 0) return -1;
	if (c->count_encoders > ne) c->count_encoders = 0;
	if (c->count_modes > nm) c->count_modes = 0;
	return 0;
}

int displayback_find(int fd, displayback_panel *p)
{
	uint32_t crtcs[DB_MAX], conns[DB_MAX], encs[DB_MAX];
	struct db_card_res res;
	uint32_t i, j, k;

	memset(p, 0, sizeof *p);
	if (fd < 0) return -1;
	memset(&res, 0, sizeof res);
	if (displayback_ioctl(fd, DB_GETRESOURCES, &res) != 0) return -1;
	if (res.count_crtcs > DB_MAX) res.count_crtcs = DB_MAX;
	if (res.count_connectors > DB_MAX) res.count_connectors = DB_MAX;
	res.count_fbs = res.count_encoders = 0;
	res.crtc_id_ptr = (uint64_t)(uintptr_t)crtcs;
	res.connector_id_ptr = (uint64_t)(uintptr_t)conns;
	res.fb_id_ptr = res.encoder_id_ptr = 0;
	if (displayback_ioctl(fd, DB_GETRESOURCES, &res) != 0) return -1;
	if (res.count_crtcs > DB_MAX) res.count_crtcs = DB_MAX;
	if (res.count_connectors > DB_MAX) res.count_connectors = DB_MAX;

	for (i = 0; i < res.count_connectors; i++) {
		struct db_connector c;
		struct db_encoder e;

		if (get_connector(fd, conns[i], &c, encs, NULL) != 0) continue;
		if (c.connection != DB_CONNECTED) continue;
		p->connector = conns[i];
		/* The CRTC its encoder shows: the one lit when SDL started */
		memset(&e, 0, sizeof e);
		e.encoder_id = c.encoder_id;
		if (c.encoder_id && displayback_ioctl(fd, DB_GETENCODER, &e) == 0 && e.crtc_id) {
			p->crtc = e.crtc_id;
			return 0;
		}
		/* None lit: the first CRTC one of its encoders can use */
		for (j = 0; j < c.count_encoders; j++) {
			memset(&e, 0, sizeof e);
			e.encoder_id = encs[j];
			if (displayback_ioctl(fd, DB_GETENCODER, &e) != 0) continue;
			for (k = 0; k < res.count_crtcs; k++)
				if (e.possible_crtcs & (1u << k)) {
					p->crtc = crtcs[k];
					return 0;
				}
		}
		p->connector = 0;
	}
	return -1;
}

/* The panel's own mode, as SDL picks it with no mode on the CRTC: the one
 * marked preferred, or the largest */
static int panel_mode(int fd, uint32_t connector, struct db_modeinfo *out)
{
	struct db_modeinfo modes[DB_MAX_MODES];
	uint32_t encs[DB_MAX], i, best = 0, area = 0;
	struct db_connector c;

	if (get_connector(fd, connector, &c, encs, modes) != 0) return -1;
	if (!c.count_modes) { errno = ENOENT; return -1; }
	for (i = 0; i < c.count_modes; i++) {
		uint32_t a = (uint32_t)modes[i].hdisplay * modes[i].vdisplay;

		if (modes[i].type & DB_MODE_PREFERRED) { best = i; break; }
		if (a > area) { best = i; area = a; }
	}
	*out = modes[best];
	return 0;
}

/* The format SDL's own frames are scanned out in: a page flip cannot change
 * format (the kernel refuses it, EINVAL), so the black picture SDL's next flip
 * replaces must be in it too. SDL draws on this same DRM file, and the file's
 * framebuffers are its frames; the first one's format, then, or 0 with none.
 * ARGB8888 on the Pixel 2, as Diatom measured for its own frames
 * (diatom/port/pixel2.c, bo_fb); XRGB8888 on QEMU's virtio-gpu. */
static uint32_t sdl_format(int fd, const displayback_panel *p)
{
	uint32_t fbs[DB_MAX_FBS], i, n;
	struct db_card_res res;

	memset(&res, 0, sizeof res);
	res.count_fbs = DB_MAX_FBS;
	res.fb_id_ptr = (uint64_t)(uintptr_t)fbs;
	if (displayback_ioctl(fd, DB_GETRESOURCES, &res) != 0) return 0;
	n = res.count_fbs < DB_MAX_FBS ? res.count_fbs : DB_MAX_FBS;
	for (i = 0; i < n; i++) {
		struct db_fb_cmd2 f;

		if (fbs[i] == p->fb) continue;     /* the black one, ours */
		memset(&f, 0, sizeof f);
		f.fb_id = fbs[i];
		if (displayback_ioctl(fd, DB_GETFB2, &f) == 0 && f.pixel_format) return f.pixel_format;
	}
	return 0;
}

/* A black picture w x h in format, made once and kept: on the panel only
 * until SDL's next flip replaces it, and ready for the next game that
 * crashes. Black because the kernel clears the memory it hands out, and zero
 * is black in ARGB8888 and XRGB8888 alike. */
static int black_picture(int fd, displayback_panel *p, uint32_t w, uint32_t h, uint32_t format)
{
	struct db_create_dumb d;
	struct db_fb_cmd2 f;

	if (p->fb && p->fb_w == w && p->fb_h == h && p->format == format) return 0;
	if (p->fb) {
		/* Another size or format: the old one is on nothing, the CRTC
		 * having no picture */
		uint32_t fb = p->fb, handle = p->handle;

		displayback_ioctl(fd, DB_RMFB, &fb);
		displayback_ioctl(fd, DB_DESTROY_DUMB, &handle);
		p->fb = p->handle = p->fb_w = p->fb_h = p->format = 0;
	}
	memset(&d, 0, sizeof d);
	d.width = w;
	d.height = h;
	d.bpp = 32;
	if (displayback_ioctl(fd, DB_CREATE_DUMB, &d) != 0) return -1;
	memset(&f, 0, sizeof f);
	f.width = w;
	f.height = h;
	f.pixel_format = format;
	f.handles[0] = d.handle;
	f.pitches[0] = d.pitch;
	if (displayback_ioctl(fd, DB_ADDFB2, &f) != 0) {
		int e = errno;
		uint32_t handle = d.handle;

		displayback_ioctl(fd, DB_DESTROY_DUMB, &handle);
		errno = e;
		return -1;
	}
	p->fb = f.fb_id;
	p->handle = d.handle;
	p->fb_w = w;
	p->fb_h = h;
	p->format = format;
	return 0;
}

/* The black picture in format, set on the panel's CRTC at mode */
static int light(int fd, displayback_panel *p, const struct db_modeinfo *mode, uint32_t format)
{
	struct db_crtc c;
	uint32_t conn = p->connector;

	if (black_picture(fd, p, mode->hdisplay, mode->vdisplay, format) != 0) return -1;
	memset(&c, 0, sizeof c);
	c.set_connectors_ptr = (uint64_t)(uintptr_t)&conn;
	c.count_connectors = 1;
	c.crtc_id = p->crtc;
	c.fb_id = p->fb;
	c.mode_valid = 1;
	c.mode = *mode;
	return displayback_ioctl(fd, DB_SETCRTC, &c);
}

int displayback_relight(int fd, displayback_panel *p)
{
	struct db_modeinfo mode;
	struct db_crtc c;
	uint32_t format;

	if (fd < 0 || !p->crtc || !p->connector) { errno = ENODEV; return -1; }
	memset(&c, 0, sizeof c);
	c.crtc_id = p->crtc;
	if (displayback_ioctl(fd, DB_GETCRTC, &c) != 0) return -1;
	if (c.mode_valid && c.fb_id) return 0;
	/* The CRTC still on with no picture keeps its mode; one the kernel
	 * turned off has none, and takes the panel's */
	if (c.mode_valid) mode = c.mode;
	else if (panel_mode(fd, p->connector, &mode) != 0) return -1;

	/* SDL's format; with no frame of SDL's to read it from, ARGB8888, and
	 * XRGB8888 on a display that refuses that (Diatom's order, show_frame) */
	format = sdl_format(fd, p);
	if (light(fd, p, &mode, format ? format : DB_ARGB8888) == 0) return 1;
	if (format || errno != EINVAL) return -1;
	return light(fd, p, &mode, DB_XRGB8888) == 0 ? 1 : -1;
}
