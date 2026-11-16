/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-panelgamma: src/panelgamma.c against a stand-in display, the
 * kernel's DRM ioctls answered here. The request numbers it answers are
 * Linux's own, written as the kernel's headers make them, so a number
 * panelgamma.c packs wrong is a refusal here as it would be on the Pixel 2.
 *
 * The cases: a table an older splash filled (emptied, on that CRTC alone,
 * GAMMA_LUT_SIZE beside it untouched); a table already empty (nothing done:
 * emptying makes the controller wait a refresh); a kernel with no GAMMA_LUT
 * (QEMU's display); no DRM master (the kernel refuses: -1); no display.
 * And the two steps under the backlight's floor (1.0.6): the table's
 * numbers; a dimming table loaded on the lit CRTC alone, as one blob the
 * kernel holds and this program lets go of; the same step again (nothing:
 * loading makes the controller show a frame with no table); the other step;
 * an older splash's table replaced; back at the floor, emptied; a table of
 * another size refused. */
#include "../src/panelgamma.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* include/uapi/drm/drm.h's numbers on Linux, as the kernel's headers give
 * them: DRM_IOWR(nr, struct ...) */
#define LINUX_GETRESOURCES 0xC04064A0ul
#define LINUX_GETPROPERTY  0xC04064AAul
#define LINUX_OBJ_GETPROPS 0xC02064B9ul
#define LINUX_OBJ_SETPROP  0xC01864BAul
#define LINUX_GETCRTC      0xC06864A1ul
#define LINUX_GETPROPBLOB  0xC01064ACul
#define LINUX_CREATEBLOB   0xC01064BDul
#define LINUX_DESTROYBLOB  0xC00464BEul
#define OBJECT_CRTC        0xccccccccu

struct card_res {
	uint64_t fb_id_ptr, crtc_id_ptr, connector_id_ptr, encoder_id_ptr;
	uint32_t count_fbs, count_crtcs, count_connectors, count_encoders;
	uint32_t min_width, max_width, min_height, max_height;
};
struct obj_props {
	uint64_t props_ptr, prop_values_ptr;
	uint32_t count_props, obj_id, obj_type;
};
struct prop {
	uint64_t values_ptr, enum_blob_ptr;
	uint32_t prop_id, flags;
	char     name[32];
	uint32_t count_values, count_enum_blobs;
};
struct set_prop {
	uint64_t value;
	uint32_t prop_id, obj_id, obj_type;
};

struct crtc_info {
	uint64_t set_connectors_ptr;
	uint32_t count_connectors, crtc_id, fb_id, x, y, gamma_size, mode_valid;
	uint8_t  mode[68];
};
struct get_blob { uint32_t blob_id, length; uint64_t data; };
struct create_blob { uint64_t data; uint32_t length, blob_id; };
struct color_lut { uint16_t red, green, blue, reserved; };

/* The kernel's blobs: 47 the older splash's table (red and green as in^1.5),
 * the rest made by panelgamma_apply; `held` while a program has it */
#define NBLOBS 16
static struct { uint32_t id; int live, held; struct color_lut lut[256]; } blobs[NBLOBS];
static int nblobs, creates, destroys;
static uint32_t next_id;

static int blob_of(uint32_t id)
{
	for (int i = 0; i < nblobs; i++)
		if (blobs[i].id == id && blobs[i].live) return i;
	return -1;
}

/* The stand-in: CRTCs with their properties' values, and the property names */
struct fake_prop { uint32_t id; const char *name; };
struct fake_crtc { uint32_t id; int nprops; uint32_t props[8]; uint64_t values[8]; };

static struct fake_prop names[] = {
	{ 20, "ACTIVE" }, { 21, "MODE_ID" }, { 22, "OUT_FENCE_PTR" }, { 23, "VRR_ENABLED" },
	{ 24, "GAMMA_LUT_SIZE" }, { 25, "GAMMA_LUT" },
};
static struct fake_crtc crtcs[2];
static int ncrtcs, master, broken, unknown;
static int sets;
static struct set_prop last_set;

static const char *name_of(uint32_t id)
{
	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
		if (names[i].id == id) return names[i].name;
	return NULL;
}

static struct fake_crtc *crtc_of(uint32_t id)
{
	for (int i = 0; i < ncrtcs; i++)
		if (crtcs[i].id == id) return &crtcs[i];
	return NULL;
}

static int fake_ioctl(int fd, unsigned long request, void *arg)
{
	(void)fd;
	if (broken) { errno = ENOTTY; return -1; }
	switch (request) {
	case LINUX_GETRESOURCES: {
		struct card_res *r = arg;
		uint32_t *ids = (uint32_t *)(uintptr_t)r->crtc_id_ptr;

		if (ids)
			for (int i = 0; i < ncrtcs && i < (int)r->count_crtcs; i++) ids[i] = crtcs[i].id;
		r->count_crtcs = (uint32_t)ncrtcs;
		return 0;
	}
	case LINUX_OBJ_GETPROPS: {
		struct obj_props *o = arg;
		struct fake_crtc *c = crtc_of(o->obj_id);
		uint32_t *ids = (uint32_t *)(uintptr_t)o->props_ptr;
		uint64_t *vals = (uint64_t *)(uintptr_t)o->prop_values_ptr;

		if (!c || o->obj_type != OBJECT_CRTC) { errno = ENOENT; return -1; }
		if (ids && vals)
			for (int i = 0; i < c->nprops && i < (int)o->count_props; i++) {
				ids[i] = c->props[i];
				vals[i] = c->values[i];
			}
		o->count_props = (uint32_t)c->nprops;
		return 0;
	}
	case LINUX_GETPROPERTY: {
		struct prop *p = arg;
		const char *n = name_of(p->prop_id);

		if (!n) { errno = ENOENT; return -1; }
		snprintf(p->name, sizeof p->name, "%s", n);
		return 0;
	}
	case LINUX_GETCRTC: {
		struct crtc_info *ci = arg;
		struct fake_crtc *c = crtc_of(ci->crtc_id);

		if (!c) { errno = ENOENT; return -1; }
		ci->mode_valid = c->values[0] != 0;   /* lit as ACTIVE says */
		ci->gamma_size = 256;
		return 0;
	}
	case LINUX_GETPROPBLOB: {
		struct get_blob *g = arg;
		int b = blob_of(g->blob_id);

		if (b < 0) { errno = ENOENT; return -1; }
		if (g->length >= sizeof blobs[b].lut && g->data)
			memcpy((void *)(uintptr_t)g->data, blobs[b].lut, sizeof blobs[b].lut);
		g->length = sizeof blobs[b].lut;
		return 0;
	}
	case LINUX_CREATEBLOB: {
		struct create_blob *cb = arg;

		if (cb->length != sizeof blobs[0].lut || nblobs == NBLOBS) { errno = EINVAL; return -1; }
		blobs[nblobs].id = next_id++;
		blobs[nblobs].live = blobs[nblobs].held = 1;
		memcpy(blobs[nblobs].lut, (const void *)(uintptr_t)cb->data, cb->length);
		cb->blob_id = blobs[nblobs].id;
		nblobs++;
		creates++;
		return 0;
	}
	case LINUX_DESTROYBLOB: {
		uint32_t id = *(uint32_t *)arg;
		int b = blob_of(id);

		if (b < 0 || !blobs[b].held) { errno = EPERM; return -1; }
		blobs[b].held = 0;
		destroys++;
		return 0;
	}
	case LINUX_OBJ_SETPROP: {
		struct set_prop *s = arg;
		struct fake_crtc *c = crtc_of(s->obj_id);

		if (!master) { errno = EACCES; return -1; }
		if (!c || s->obj_type != OBJECT_CRTC) { errno = ENOENT; return -1; }
		for (int i = 0; i < c->nprops; i++)
			if (c->props[i] == s->prop_id) {
				c->values[i] = s->value;
				sets++;
				last_set = *s;
				return 0;
			}
		errno = EINVAL;
		return -1;
	}
	default:
		unknown++;
		errno = EINVAL;
		return -1;
	}
}

/* The Pixel 2's two VOPs as plastron's kernel has them: the big one lit with
 * the splash's table (blob 47) when `table`, the little one never lit; with
 * `has_lut` 0, a kernel with no GAMMA_LUT at all */
static void pixel2(int table, int has_lut)
{
	static const uint32_t with[] = { 20, 21, 22, 23, 24, 25 };
	static const uint32_t without[] = { 20, 21, 22, 23 };

	memset(crtcs, 0, sizeof crtcs);
	ncrtcs = 2;
	crtcs[0].id = 49;
	crtcs[1].id = 67;
	for (int c = 0; c < 2; c++) {
		const uint32_t *p = has_lut ? with : without;
		int n = has_lut ? 6 : 4;

		crtcs[c].nprops = n;
		for (int i = 0; i < n; i++) crtcs[c].props[i] = p[i];
		crtcs[c].values[0] = c == 0;                  /* ACTIVE */
		if (has_lut) crtcs[c].values[4] = 256;        /* GAMMA_LUT_SIZE */
	}
	if (table && has_lut) crtcs[0].values[5] = 47;
	memset(blobs, 0, sizeof blobs);
	blobs[0].id = 47;
	blobs[0].live = 1;
	for (int i = 0; i < 256; i++) {
		uint16_t rg = (uint16_t)(65535.0 * pow(i / 255.0, 1.5) + 0.5);

		blobs[0].lut[i] = (struct color_lut){ rg, rg, (uint16_t)(i * 257), 0 };
	}
	nblobs = 1;
	next_id = 100;
	creates = destroys = 0;
	master = 1;
	broken = 0;
	unknown = 0;
	sets = 0;
	memset(&last_set, 0, sizeof last_set);
}

static int fails;

#define CHECK(cond, what) do { \
	if (cond) printf("  ok   %s\n", what); \
	else { printf("  FAIL %s\n", what); fails++; } \
} while (0)

int main(void)
{
	int n;

	printf("check-panelgamma:\n");
	panelgamma_ioctl = fake_ioctl;

	pixel2(1, 1);
	n = panelgamma_clear(3);
	CHECK(n == 1, "an older splash's table: emptied, and said so (1)");
	CHECK(sets == 1 && last_set.obj_id == 49 && last_set.obj_type == OBJECT_CRTC &&
	      last_set.prop_id == 25 && last_set.value == 0,
	      "  ...the lit CRTC's GAMMA_LUT set to no blob, nothing else set");
	CHECK(crtcs[0].values[4] == 256 && crtcs[1].values[5] == 0,
	      "  ...GAMMA_LUT_SIZE beside it and the other CRTC left as they were");
	CHECK(unknown == 0, "  ...every request a number Linux knows");
	n = panelgamma_clear(3);
	CHECK(n == 0 && sets == 1, "  ...and asked again, nothing more to do");

	pixel2(0, 1);
	n = panelgamma_clear(3);
	CHECK(n == 0 && sets == 0, "a table already empty (this splash): left alone, no refresh's wait");

	pixel2(1, 0);
	n = panelgamma_clear(3);
	CHECK(n == 0 && sets == 0, "a kernel with no GAMMA_LUT (QEMU's display): nothing to do");

	pixel2(1, 1);
	master = 0;
	n = panelgamma_clear(3);
	CHECK(n == -1 && crtcs[0].values[5] == 47, "without DRM master the kernel refuses: -1, the table as it was");

	pixel2(1, 1);
	broken = 1;
	CHECK(panelgamma_clear(3) == -1, "not a display (the ioctls refused): -1");
	broken = 0;
	CHECK(panelgamma_clear(-1) == -1 && sets == 0, "no display at all (fd -1): -1, nothing asked");

	/* Under the floor */
	CHECK(panelgamma_out(13, 255) == 226 && panelgamma_out(9, 255) == 191 &&
	      panelgamma_out(13, 128) == 113 && panelgamma_out(9, 0) == 0,
	      "the table under the floor: 255 -> 226 at 13 and 191 at 9 (light 76% and 53%), 0 stays 0");
	{
		int same = 1;

		for (int i = 0; i < 256; i++)
			same &= panelgamma_out(17, i) == i && panelgamma_out(255, i) == i &&
			        panelgamma_out(0, i) == i;
		CHECK(same, "  ...and from the floor up, and with the light off, each value itself");
	}

	pixel2(0, 1);
	n = panelgamma_apply(3, 13);
	CHECK(n == 1 && sets == 1 && last_set.obj_id == 49 && last_set.prop_id == 25 &&
	      last_set.value == 100 && crtcs[1].values[5] == 0,
	      "13: a dimming table on the lit CRTC alone, as one new blob");
	{
		int b = blob_of(100), ok = b >= 0;

		for (int i = 0; ok && i < 256; i++)
			ok = blobs[b].lut[i].red == (uint16_t)(panelgamma_out(13, i) << 8) &&
			     blobs[b].lut[i].green == blobs[b].lut[i].red &&
			     blobs[b].lut[i].blue == blobs[b].lut[i].red && !blobs[b].lut[i].reserved;
		CHECK(ok, "  ...its entries the table's values in the top byte, all three channels alike");
	}
	CHECK(destroys == 1 && !blobs[blob_of(100)].held, "  ...and this program's hold on the blob let go");
	CHECK(unknown == 0, "  ...every request a number Linux knows");
	n = panelgamma_apply(3, 13);
	CHECK(n == 0 && sets == 1 && creates == 1, "13 again: the table already there, nothing loaded");
	n = panelgamma_apply(3, 9);
	CHECK(n == 1 && sets == 2 && crtcs[0].values[5] == 101 &&
	      blobs[blob_of(101)].lut[255].red == 191 << 8, "9: the other step's table");
	n = panelgamma_apply(3, 17);
	CHECK(n == 1 && crtcs[0].values[5] == 0, "back at the floor: emptied");
	n = panelgamma_apply(3, 122);
	CHECK(n == 0 && sets == 3, "  ...and above it, nothing more");

	pixel2(1, 1);
	n = panelgamma_apply(3, 9);
	CHECK(n == 1 && crtcs[0].values[5] == 100, "9 over an older splash's table: replaced");

	pixel2(0, 1);
	crtcs[0].values[4] = 1024;
	n = panelgamma_apply(3, 9);
	CHECK(n == -1 && sets == 0 && creates == 0, "a table of another size than the VOP's: refused, nothing set");

	pixel2(0, 1);
	master = 0;
	n = panelgamma_apply(3, 9);
	CHECK(n == -1 && crtcs[0].values[5] == 0 && destroys == creates,
	      "without DRM master: -1, no table, no blob kept");

	/* Screen Colors */
	{
		const panelgamma_color std = { 0, 0, 0 }, night3 = { 3, 0, 0 }, night5 = { 5, 0, 0 },
		                       con2 = { 0, 2, 0 }, vivid = { 0, 1, 1 }, all = { 5, 2, 1 };
		const panelgamma_color *every[] = { &night3, &night5, &con2, &vivid, &all };
		int same = 1, mono = 1, ends = 1, warm = 1;

		for (int raw = 0; raw <= 255; raw++)
			for (int ch = 0; ch < 3; ch++)
				for (int i = 0; i < 256; i++)
					same &= panelgamma_color_out(raw, &std, ch, i) == panelgamma_out(raw, i) &&
					        panelgamma_color_out(raw, NULL, ch, i) == panelgamma_out(raw, i);
		CHECK(same && panelgamma_color_standard(&std) && panelgamma_color_standard(NULL),
		      "Standard (all 0, or none): the table exactly as without Screen Colors, every step");
		for (unsigned k = 0; k < sizeof every / sizeof every[0]; k++)
			for (int ch = 0; ch < 3; ch++) {
				ends &= panelgamma_color_out(122, every[k], ch, 0) == 0;
				for (int i = 1; i < 256; i++)
					mono &= panelgamma_color_out(122, every[k], ch, i) >=
					        panelgamma_color_out(122, every[k], ch, i - 1);
			}
		CHECK(mono && ends, "every setting's table rises with its input (no band turns over) and keeps black black");
		CHECK(panelgamma_color_out(122, &night3, 0, 255) == 255 && panelgamma_color_out(122, &night3, 1, 255) == 241 &&
		      panelgamma_color_out(122, &night3, 2, 255) == 213,
		      "Night Light 3: white 255,241,213 (green's light 88%, blue's 67%)");
		for (int i = 1; i < 256; i++)
			warm &= panelgamma_color_out(122, &night5, 0, i) >= panelgamma_color_out(122, &night5, 1, i) &&
			        panelgamma_color_out(122, &night5, 1, i) >= panelgamma_color_out(122, &night5, 2, i);
		CHECK(warm && panelgamma_color_out(122, &night5, 2, 255) < panelgamma_color_out(122, &night3, 2, 255),
		      "  ...every grey warmer (red >= green >= blue), and 5 warmer than 3");
		CHECK(panelgamma_color_out(122, &con2, 0, 255) == 255 && panelgamma_color_out(122, &con2, 0, 128) == 128 &&
		      panelgamma_color_out(122, &con2, 0, 64) < 64 && panelgamma_color_out(122, &con2, 0, 192) > 192 &&
		      panelgamma_color_out(122, &con2, 2, 64) == panelgamma_color_out(122, &con2, 0, 64),
		      "Contrast +2: white and the middle kept, shadows darker, highlights brighter, every channel alike");
		CHECK(panelgamma_color_out(122, &vivid, 0, 255) == 255 &&
		      panelgamma_color_out(122, &vivid, 0, 48) < panelgamma_color_out(122, &(panelgamma_color){ 0, 1, 0 }, 0, 48),
		      "Vivid: deeper shadows than its Contrast +1 alone, white kept");
		CHECK(panelgamma_color_out(13, &night3, 0, 255) == 226 && panelgamma_color_out(9, &night3, 0, 255) == 191 &&
		      panelgamma_color_out(13, &night3, 2, 255) < panelgamma_color_out(122, &night3, 2, 255),
		      "  ...and under the floor each on top of the dimming: red 255 -> 226 at 13 as before, blue lower still");

		pixel2(0, 1);
		n = panelgamma_apply_color(3, 122, &night3);
		CHECK(n == 1 && crtcs[0].values[5] == 100 && crtcs[1].values[5] == 0,
		      "Night Light 3 above the floor: a table on the lit CRTC (the backlight alone made no table before)");
		{
			int b = blob_of(100);

			CHECK(b >= 0 && blobs[b].lut[255].red == 255 << 8 && blobs[b].lut[255].green == 241 << 8 &&
			      blobs[b].lut[255].blue == 213 << 8, "  ...its channels apart: red, green, blue each its own");
		}
		n = panelgamma_apply_color(3, 122, &night3);
		CHECK(n == 0 && creates == 1, "  ...asked again: nothing loaded, no blink");
		n = panelgamma_apply_color(3, 13, &night3);
		CHECK(n == 1 && blobs[blob_of(101)].lut[255].red == 226 << 8, "  ...a step under the floor: both in one table");
		n = panelgamma_apply_color(3, 122, &std);
		CHECK(n == 1 && crtcs[0].values[5] == 0, "back to Standard above the floor: no table, as before");
	}

	/* The file the launcher leaves for Diatom and portkeys */
	{
		char dir[] = "/tmp/zyperos-pg-XXXXXX", path[64];
		panelgamma_color c = { 9, 9, 9 }, w = { 2, 1, 1 };

		if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
		snprintf(path, sizeof path, "%s/screen", dir);
		CHECK(panelgamma_color_read(path, &c) == 0 && panelgamma_color_standard(&c),
		      "no file (a cold boot before the launcher): Standard");
		CHECK(panelgamma_color_write(path, &w) == 0 && panelgamma_color_read(path, &c) == 1 &&
		      c.night == 2 && c.contrast == 1 && c.vivid == 1, "written and read back");
		{
			FILE *f = fopen(path, "w");

			fputs("night=9 contrast=-4 bogus vivid=7\n", f);
			fclose(f);
		}
		CHECK(panelgamma_color_read(path, &c) == 1 && c.night == PANELGAMMA_NIGHT_MAX && c.contrast == 0 &&
		      c.vivid == 1, "a line out of range: each value put in range, a word it does not know passed over");
		{
			FILE *f = fopen(path, "w");

			fclose(f);
		}
		CHECK(panelgamma_color_read(path, &c) == 1 && panelgamma_color_standard(&c), "an empty file: Standard");
		remove(path);
		CHECK(mkfifo(path, 0644) == 0 && panelgamma_color_read(path, &c) == 1 && panelgamma_color_standard(&c),
		      "a FIFO at the name: Standard at once, no wait for a writer");
		CHECK(panelgamma_color_write(path, &w) == 0 && panelgamma_color_read(path, &c) == 1 && c.night == 2,
		      "  ...and a write puts a file in its place");
		{
			char tmp[80];

			snprintf(tmp, sizeof tmp, "%s.tmp", path);
			CHECK(symlink("/nonexistent-zyperos-pg-target", tmp) == 0 && panelgamma_color_write(path, &w) == 0 &&
			      access("/nonexistent-zyperos-pg-target", F_OK) != 0 && access(tmp, F_OK) != 0,
			      "a link left at the temporary's name: not followed, the write made all the same");
		}
		remove(path);
		rmdir(dir);
	}

	if (fails) {
		printf("check-panelgamma: %d failed\n", fails);
		return 1;
	}
	printf("check-panelgamma: all passed\n");
	return 0;
}
