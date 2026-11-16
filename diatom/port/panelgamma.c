/* SPDX-License-Identifier: GPL-3.0-only OR MPL-2.0 */
/* Vendored from ZyperOS src/panelgamma.c, which is where it is changed: the
 * launcher and this port set the display's gamma table with the same code.
 * Copy it back here whole, below this note, when it changes there: make check
 * fails while the two differ (tools/check-copies.py). */
/* The display controller's gamma table, empty or dimming. See src/panelgamma.h. */
#include "panelgamma.h"

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>

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

int panelgamma_out(int raw, int in)
{
	if (in <= 0) return 0;
	if (in > 255) in = 255;
	if (raw <= 0 || raw >= PANELGAMMA_FLOOR) return in;
	return (int)lround(in * pow((double)raw / PANELGAMMA_FLOOR, 1 / 2.2));
}

/* The table for raw, as the kernel reads it: 16 bits an entry, of which the
 * VOP keeps the top 8 (drm_color_lut_extract rounds there), so each output
 * is put in the top byte and comes back out exactly. */
static void fill(struct pg_color_lut *lut, int raw)
{
	int i;

	for (i = 0; i < PG_LUT; i++) {
		uint16_t v = (uint16_t)(panelgamma_out(raw, i) << 8);

		lut[i].red = lut[i].green = lut[i].blue = v;
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

/* CRTC crtc's GAMMA_LUT as raw wants it: 1 changed, 0 nothing to change,
 * -1 failed. */
static int apply_one(int fd, uint32_t crtc, int raw)
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

	if (raw <= 0 || raw >= PANELGAMMA_FLOOR) {
		if (lut_now == 0) return 0;    /* empty already */
		return set_lut(fd, crtc, lut_prop, 0) == 0 ? 1 : -1;
	}

	/* A dimming table: only on a controller showing a picture (the Pixel 2's
	 * second VOP drives nothing), and whose table is the VOP's size */
	{
		struct pg_crtc c;

		memset(&c, 0, sizeof c);
		c.crtc_id = crtc;
		if (panelgamma_ioctl(fd, PG_GETCRTC, &c) != 0) return -1;
		if (!c.mode_valid) return 0;
	}
	if (lut_size != PG_LUT) return -1;
	fill(want, raw);
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
		int r = apply_one(fd, crtcs[i], raw);

		if (r > 0) changed++;
		else if (r < 0) failed = 1;
	}
	return failed && !changed ? -1 : changed;
}

int panelgamma_clear(int fd)
{
	return panelgamma_apply(fd, PANELGAMMA_FLOOR);
}
