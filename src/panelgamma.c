/* SPDX-License-Identifier: GPL-3.0-only OR MPL-2.0 */
/* The display controller's gamma table, empty or dimming. See src/panelgamma.h. */
#include "panelgamma.h"

#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Linux's _IOWR('d', nr, type): read and write, the structure's size, DRM's
 * letter. Written out because a Mac's _IOWR packs the size differently. */
#define PG_IOWR(nr, size) \
	((unsigned long)(3u << 30 | (unsigned)(size) << 16 | 'd' << 8 | (nr)))

/* From the kernel's include/uapi/drm/drm_mode.h, field for field */
struct pg_card_res {                   /* DRM_IOCTL_MODE_GETRESOURCES */
	uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
	uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
	uint32_t min_width, max_width, min_height, max_height;
};
struct pg_obj_props {                  /* DRM_IOCTL_MODE_OBJ_GETPROPERTIES */
	uint64_t props_ptr, prop_values_ptr;
	uint32_t count_props, obj_id, obj_type;
};
struct pg_prop {                       /* DRM_IOCTL_MODE_GETPROPERTY */
	uint64_t values_ptr, enum_blob_ptr;
	uint32_t prop_id, flags;
	char     name[32];
	uint32_t count_values, count_enum_blobs;
};
struct pg_set_prop {                   /* DRM_IOCTL_MODE_OBJ_SETPROPERTY */
	uint64_t value;
	uint32_t prop_id, obj_id, obj_type;
};
struct pg_get_blob {                   /* DRM_IOCTL_MODE_GETPROPBLOB */
	uint32_t blob_id, length;
	uint64_t data;
};
struct pg_create_blob {                /* DRM_IOCTL_MODE_CREATEPROPBLOB */
	uint64_t data;
	uint32_t length, blob_id;
};
struct pg_destroy_blob {               /* DRM_IOCTL_MODE_DESTROYPROPBLOB */
	uint32_t blob_id;
};
struct pg_crtc {                       /* DRM_IOCTL_MODE_GETCRTC */
	uint64_t set_connectors_ptr;
	uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
	uint8_t  mode[68];                 /* struct drm_mode_modeinfo, unread */
};
struct pg_color_lut {                  /* struct drm_color_lut, a table's entry */
	uint16_t red, green, blue, reserved;
};
_Static_assert(sizeof(struct pg_card_res) == 64, "drm_mode_card_res");
_Static_assert(sizeof(struct pg_obj_props) == 32, "drm_mode_obj_get_properties");
_Static_assert(sizeof(struct pg_prop) == 64, "drm_mode_get_property");
_Static_assert(sizeof(struct pg_set_prop) == 24, "drm_mode_obj_set_property");
_Static_assert(sizeof(struct pg_get_blob) == 16, "drm_mode_get_blob");
_Static_assert(sizeof(struct pg_create_blob) == 16, "drm_mode_create_blob");
_Static_assert(sizeof(struct pg_destroy_blob) == 4, "drm_mode_destroy_blob");
_Static_assert(sizeof(struct pg_crtc) == 104, "drm_mode_crtc");
_Static_assert(sizeof(struct pg_color_lut) == 8, "drm_color_lut");

#define PG_GETRESOURCES   PG_IOWR(0xA0, sizeof(struct pg_card_res))
#define PG_GETCRTC        PG_IOWR(0xA1, sizeof(struct pg_crtc))
#define PG_GETPROPERTY    PG_IOWR(0xAA, sizeof(struct pg_prop))
#define PG_GETPROPBLOB    PG_IOWR(0xAC, sizeof(struct pg_get_blob))
#define PG_OBJ_GETPROPS   PG_IOWR(0xB9, sizeof(struct pg_obj_props))
#define PG_OBJ_SETPROP    PG_IOWR(0xBA, sizeof(struct pg_set_prop))
#define PG_CREATEPROPBLOB PG_IOWR(0xBD, sizeof(struct pg_create_blob))
#define PG_DESTROYPROPBLOB PG_IOWR(0xBE, sizeof(struct pg_destroy_blob))
#define PG_OBJECT_CRTC    0xccccccccu

static int sys_ioctl(int fd, unsigned long request, void *arg)
{
	return ioctl(fd, request, arg);
}

panelgamma_ioctl_fn panelgamma_ioctl = sys_ioctl;

#define PG_MAX 8                       /* CRTCs; the Pixel 2 has 2 */
#define PG_MAX_PROPS 64                /* a CRTC's properties; the VOP's have 9 */
#define PG_LUT 256                     /* the VOP's table, and the only size made */
#define PG_PI 3.14159265358979323846

int panelgamma_out(int raw, int in)
{
	if (in <= 0) return 0;
	if (in > 255) in = 255;
	if (raw <= 0 || raw >= PANELGAMMA_FLOOR) return in;
	return (int)lround(in * pow((double)raw / PANELGAMMA_FLOOR, 1 / 2.2));
}

int panelgamma_color_standard(const panelgamma_color *c)
{
	return !c || (!c->temp && !c->tint && !c->contrast && !c->gamma && c->night <= 0 && c->lift <= 0);
}

static int pg_clamp(int v)
{
	return v < -PANELGAMMA_RANGE ? -PANELGAMMA_RANGE : v > PANELGAMMA_RANGE ? PANELGAMMA_RANGE : v;
}

void panelgamma_color_clamp(panelgamma_color *c)
{
	c->temp = pg_clamp(c->temp);
	c->tint = pg_clamp(c->tint);
	c->contrast = pg_clamp(c->contrast);
	c->gamma = pg_clamp(c->gamma);
	c->night = c->night < 0 ? 0 : c->night > PANELGAMMA_NIGHT_MAX ? PANELGAMMA_NIGHT_MAX : c->night;
	c->lift = c->lift < 0 ? 0 : c->lift > PANELGAMMA_LIFT_MAX ? PANELGAMMA_LIFT_MAX : c->lift;
}

int panelgamma_night_at(const panelgamma_night *n, long long now)
{
	int i;

	if (!n || n->warmth <= 0) return 0;
	if (n->always) return n->warmth;
	for (i = 0; i < n->n && i < PANELGAMMA_NIGHTS; i++)
		if (now >= n->from[i] && now < n->until[i]) return n->warmth;
	return 0;
}

long long panelgamma_night_next(const panelgamma_night *n, long long now)
{
	long long next = 0;
	int i;

	if (!n || n->warmth <= 0 || n->always) return 0;
	for (i = 0; i < n->n && i < PANELGAMMA_NIGHTS; i++) {
		if (n->from[i] > now && (!next || n->from[i] < next)) next = n->from[i];
		if (n->until[i] > now && (!next || n->until[i] < next)) next = n->until[i];
	}
	return next;
}

/* "always", or up to PANELGAMMA_NIGHTS "from-until" with commas between;
 * a night that ends before it starts is none */
static void nights_parse(const char *s, panelgamma_night *n)
{
	if (!strcmp(s, "always")) {
		n->always = 1;
		return;
	}
	while (*s && n->n < PANELGAMMA_NIGHTS) {
		long long a, b;
		int used = 0;

		if (sscanf(s, "%lld-%lld%n", &a, &b, &used) != 2 || used <= 0) return;
		if (b > a) {
			n->from[n->n] = a;
			n->until[n->n] = b;
			n->n++;
		}
		s += used;
		if (*s != ',') return;
		s++;
	}
}

int panelgamma_screen_read(const char *path, panelgamma_color *c, panelgamma_night *night)
{
	char line[256], *p, *save = NULL;
	ssize_t n;
	int fd;

	memset(c, 0, sizeof *c);
	if (night) memset(night, 0, sizeof *night);
	/* As src/themefile.c reads its word: O_NONBLOCK, so a FIFO left at this
	 * name answers at once with nothing (Standard) rather than holding the
	 * display's next frame; one read, and a file this long is not ours */
	if (!path || (fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK)) < 0) return 0;
	n = read(fd, line, sizeof line - 1);
	close(fd);
	if (n < 0) return 0;
	line[n] = '\0';
	for (p = strtok_r(line, " \t\r\n", &save); p; p = strtok_r(NULL, " \t\r\n", &save)) {
		int v;

		if (sscanf(p, "temp=%d", &v) == 1) c->temp = v;
		else if (sscanf(p, "tint=%d", &v) == 1) c->tint = v;
		else if (sscanf(p, "contrast=%d", &v) == 1) c->contrast = v;
		else if (sscanf(p, "gamma=%d", &v) == 1) c->gamma = v;
		else if (night && sscanf(p, "warmth=%d", &v) == 1) night->warmth = v;
		else if (night && !strncmp(p, "nights=", 7)) nights_parse(p + 7, night);
	}
	panelgamma_color_clamp(c);
	if (night && (night->warmth < 0 || night->warmth > PANELGAMMA_NIGHT_MAX))
		night->warmth = night->warmth < 0 ? 0 : PANELGAMMA_NIGHT_MAX;
	return 1;
}

int panelgamma_color_read(const char *path, panelgamma_color *c)
{
	return panelgamma_screen_read(path, c, NULL);
}

int panelgamma_screen_write(const char *path, const panelgamma_color *c, const panelgamma_night *night)
{
	panelgamma_color v = *c;
	char tmp[256], text[256];
	int fd, len, ok, i;

	panelgamma_color_clamp(&v);
	if (!path || snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return -1;
	len = snprintf(text, sizeof text, "temp=%d tint=%d contrast=%d gamma=%d",
	               v.temp, v.tint, v.contrast, v.gamma);
	/* Night Mode, when it has a warmth and a time to be on */
	if (night && night->warmth > 0 && (night->always || night->n > 0)) {
		len += snprintf(text + len, sizeof text - (size_t)len, " warmth=%d nights=",
		                night->warmth > PANELGAMMA_NIGHT_MAX ? PANELGAMMA_NIGHT_MAX : night->warmth);
		if (night->always)
			len += snprintf(text + len, sizeof text - (size_t)len, "always");
		else
			for (i = 0; i < night->n && i < PANELGAMMA_NIGHTS; i++)
				len += snprintf(text + len, sizeof text - (size_t)len, "%s%lld-%lld",
				                i ? "," : "", night->from[i], night->until[i]);
	}
	len += snprintf(text + len, sizeof text - (size_t)len, "\n");
	/* A new temporary each time, as src/themefile.c makes its own: a link
	 * left at this name in /tmp is not followed (O_EXCL after the unlink),
	 * and a race lost there fails the write, which the caller logs */
	unlink(tmp);
	fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0644);
	if (fd < 0) return -1;
	ok = write(fd, text, (size_t)len) == len;
	ok = close(fd) == 0 && ok;
	if (!ok || rename(tmp, path) != 0) {
		unlink(tmp);
		return -1;
	}
	return 0;
}

int panelgamma_color_write(const char *path, const panelgamma_color *c)
{
	return panelgamma_screen_write(path, c, NULL);
}

/* A channel's gain in light for Temperature, Tint and Night Mode, every gain
 * 1 or under so white's brightest channel stays 255. Warm takes green a
 * little down and blue more, toward a candle's white (+10: green 80%, blue
 * 45%); cool takes red down and green a little (-10: red 80%, green 88%).
 * Magenta takes green down, green red and blue (+-10: 80%). Night is warm
 * again, multiplied in. */
static double color_gain(const panelgamma_color *v, int ch)
{
	double g = 1;

	if (v->temp > 0) {
		if (ch == 1) g *= 1 - 0.02 * v->temp;
		if (ch == 2) g *= 1 - 0.055 * v->temp;
	} else if (v->temp < 0) {
		if (ch == 0) g *= 1 + 0.02 * v->temp;
		if (ch == 1) g *= 1 + 0.012 * v->temp;
	}
	if (v->tint > 0) {
		if (ch == 1) g *= 1 - 0.02 * v->tint;
	} else if (v->tint < 0) {
		if (ch != 1) g *= 1 + 0.02 * v->tint;
	}
	/* Night Mode: Temperature's warm gains again, on top */
	if (v->night > 0) {
		if (ch == 1) g *= 1 - 0.02 * v->night;
		if (ch == 2) g *= 1 - 0.055 * v->night;
	}
	return g;
}

int panelgamma_color_out(int raw, const panelgamma_color *c, int ch, int in)
{
	panelgamma_color v;
	double x, g;

	if (panelgamma_color_standard(c)) return panelgamma_out(raw, in);
	if (in <= 0) return 0;
	if (in > 255) in = 255;
	v = *c;
	panelgamma_color_clamp(&v);
	x = in / 255.0;
	/* Gamma: the mid-tones lighter or darker, black and white kept
	 * (x^0.6 at +10, x^1.4 at -10) */
	if (v.gamma) x = pow(x, 1 - 0.04 * v.gamma);
	/* The brightness boost: the same lift again, three of Gamma's steps a
	 * cell, after the player's own */
	if (v.lift) x = pow(x, 1 - 0.12 * v.lift);
	/* Contrast: an S about the middle, steeper there and flatter at the ends
	 * (an inverse S below 0), 0, 128 and 255 kept; 5% of the slope a step,
	 * so the curve always rises */
	if (v.contrast) x -= 0.05 * v.contrast * sin(2 * PG_PI * x) / (2 * PG_PI);
	if (x < 0) x = 0;
	if (x > 1) x = 1;
	g = color_gain(&v, ch);
	if (raw > 0 && raw < PANELGAMMA_FLOOR) g *= (double)raw / PANELGAMMA_FLOOR;
	x = pow(x, 2.2) * g;
	if (x > 1) x = 1;
	return (int)lround(255 * pow(x, 1 / 2.2));
}

/* The table for raw and c, as the kernel reads it: 16 bits an entry, of
 * which the VOP keeps the top 8 (drm_color_lut_extract rounds there), so
 * each output is put in the top byte and comes back out exactly. */
static void fill(struct pg_color_lut *lut, int raw, const panelgamma_color *c)
{
	int i;

	for (i = 0; i < PG_LUT; i++) {
		lut[i].red = (uint16_t)(panelgamma_color_out(raw, c, 0, i) << 8);
		lut[i].green = (uint16_t)(panelgamma_color_out(raw, c, 1, i) << 8);
		lut[i].blue = (uint16_t)(panelgamma_color_out(raw, c, 2, i) << 8);
		lut[i].reserved = 0;
	}
}

/* Does blob `id` hold exactly `want`? */
static int holds(int fd, uint32_t id, const struct pg_color_lut *want)
{
	struct pg_color_lut got[PG_LUT];
	struct pg_get_blob b;

	memset(&b, 0, sizeof b);
	b.blob_id = id;
	b.length = sizeof got;
	b.data = (uint64_t)(uintptr_t)got;
	if (panelgamma_ioctl(fd, PG_GETPROPBLOB, &b) != 0) return 0;
	return b.length == sizeof got && !memcmp(got, want, sizeof got);
}

static int set_lut(int fd, uint32_t crtc, uint32_t prop, uint64_t value)
{
	struct pg_set_prop set;

	memset(&set, 0, sizeof set);
	set.value = value;
	set.prop_id = prop;
	set.obj_id = crtc;
	set.obj_type = PG_OBJECT_CRTC;
	return panelgamma_ioctl(fd, PG_OBJ_SETPROP, &set);
}

/* CRTC crtc's GAMMA_LUT as raw and c want it: 1 changed, 0 nothing to
 * change, -1 failed. */
static int apply_one(int fd, uint32_t crtc, int raw, const panelgamma_color *c)
{
	uint32_t ids[PG_MAX_PROPS], lut_prop = 0;
	uint64_t values[PG_MAX_PROPS], lut_now = 0, lut_size = 0;
	struct pg_obj_props op;
	struct pg_color_lut want[PG_LUT];
	struct pg_create_blob cb;
	struct pg_destroy_blob db;
	uint32_t i;
	int r;

	memset(&op, 0, sizeof op);
	op.obj_id = crtc;
	op.obj_type = PG_OBJECT_CRTC;
	if (panelgamma_ioctl(fd, PG_OBJ_GETPROPS, &op) != 0) return -1;
	if (op.count_props > PG_MAX_PROPS) op.count_props = PG_MAX_PROPS;
	op.props_ptr = (uint64_t)(uintptr_t)ids;
	op.prop_values_ptr = (uint64_t)(uintptr_t)values;
	if (panelgamma_ioctl(fd, PG_OBJ_GETPROPS, &op) != 0) return -1;
	if (op.count_props > PG_MAX_PROPS) op.count_props = PG_MAX_PROPS;

	for (i = 0; i < op.count_props; i++) {
		struct pg_prop p;

		memset(&p, 0, sizeof p);
		p.prop_id = ids[i];
		if (panelgamma_ioctl(fd, PG_GETPROPERTY, &p) != 0) continue;
		p.name[sizeof p.name - 1] = '\0';
		if (!strcmp(p.name, "GAMMA_LUT")) {
			lut_prop = ids[i];
			lut_now = values[i];
		} else if (!strcmp(p.name, "GAMMA_LUT_SIZE")) {
			lut_size = values[i];
		}
	}
	if (!lut_prop) return 0;           /* no GAMMA_LUT on this kernel */

	if ((raw <= 0 || raw >= PANELGAMMA_FLOOR) && panelgamma_color_standard(c)) {
		if (lut_now == 0) return 0;    /* empty already */
		return set_lut(fd, crtc, lut_prop, 0) == 0 ? 1 : -1;
	}

	/* A table: only on a controller showing a picture (the Pixel 2's
	 * second VOP drives nothing), and whose table is the VOP's size */
	{
		struct pg_crtc c;

		memset(&c, 0, sizeof c);
		c.crtc_id = crtc;
		if (panelgamma_ioctl(fd, PG_GETCRTC, &c) != 0) return -1;
		if (!c.mode_valid) return 0;
	}
	if (lut_size != PG_LUT) return -1;
	fill(want, raw, c);
	if (lut_now != 0 && holds(fd, (uint32_t)lut_now, want)) return 0;

	memset(&cb, 0, sizeof cb);
	cb.data = (uint64_t)(uintptr_t)want;
	cb.length = sizeof want;
	if (panelgamma_ioctl(fd, PG_CREATEPROPBLOB, &cb) != 0) return -1;
	r = set_lut(fd, crtc, lut_prop, cb.blob_id) == 0 ? 1 : -1;
	/* The CRTC keeps its own hold on the table; ours goes either way */
	memset(&db, 0, sizeof db);
	db.blob_id = cb.blob_id;
	panelgamma_ioctl(fd, PG_DESTROYPROPBLOB, &db);
	return r;
}

int panelgamma_apply(int fd, int raw)
{
	return panelgamma_apply_color(fd, raw, NULL);
}

int panelgamma_apply_color(int fd, int raw, const panelgamma_color *c)
{
	uint32_t crtcs[PG_MAX];
	struct pg_card_res res;
	uint32_t i;
	int changed = 0, failed = 0;

	if (fd < 0) return -1;
	memset(&res, 0, sizeof res);
	if (panelgamma_ioctl(fd, PG_GETRESOURCES, &res) != 0) return -1;
	if (res.count_crtcs > PG_MAX) res.count_crtcs = PG_MAX;
	res.crtc_id_ptr = (uint64_t)(uintptr_t)crtcs;
	res.count_fbs = res.count_connectors = res.count_encoders = 0;
	if (panelgamma_ioctl(fd, PG_GETRESOURCES, &res) != 0) return -1;
	if (res.count_crtcs > PG_MAX) res.count_crtcs = PG_MAX;

	for (i = 0; i < res.count_crtcs; i++) {
		int r = apply_one(fd, crtcs[i], raw, c);

		if (r > 0) changed++;
		else if (r < 0) failed = 1;
	}
	return failed && !changed ? -1 : changed;
}

int panelgamma_clear(int fd)
{
	return panelgamma_apply(fd, PANELGAMMA_FLOOR);
}
