/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * portout.h: a port's latest output on its loading screen, for splash
 * --still (ZyperOS's DESIGN.md 11.11). Header only and with no DRM in it, so
 * ZyperOS's tools/portout-check.c reads and draws with this same code on a
 * computer.
 *
 * A first start of a port can install for hours (a runtime downloaded, a GOG
 * installer unpacked: 5 hours for one player's Dead Cells), and the loading
 * screen showed nothing of it, so the player took it for a hang and stopped
 * it. What the port's script and PortMaster print goes to ZyperOS's log
 * (launch.sh), and splash follows the log a few times a second and draws its
 * latest line at the screen's foot.
 *
 * THE LINE (portout_feed, portout_shown). The bytes the log gains, read as a
 * terminal shows them: a carriage return starts the line again, so a
 * progress line rewritten in place shows as it is now, and "\r\n" is a line
 * end; a backspace takes the last letter back; a tab is a space; escape
 * sequences (colors, cursor moves) and every other control character are
 * dropped, and so is a letter the font has none of. Shown: the line being
 * written when it has letters, else the last whole line that had. Runs of
 * spaces are one, ends trimmed. Lines of ZyperOS's own programs (portkeys:,
 * zyhome:, port:, kernel:) are not the port's and are passed over.
 *
 * THE LETTERS AND THE BOX (portout_load). ZyperOS's launcher writes them as
 * it saves the loading screen (its src/portload.c, port_out_write), in the
 * screen's own type at the panel's size and in the screen's color, since it
 * has the fonts and the theme and splash has neither:
 *
 *   "ZPOU", u16 version (1), u16 the screen's width and height in panel
 *   pixels, landscape (640 x 480); u16 x, y, w, h, the line's box in them;
 *   u8 red, green, blue and a zero, the ink; u16 the count of letters; then
 *   each letter: u32 its code point, u16 its advance, s16 where its cell
 *   starts from the pen, u16 its cell's width, then the cell's coverage,
 *   width x h bytes, row by row. All little-endian.
 *
 * DRAWN (portout_draw) over the box's own pixels of the screen, so a shorter
 * line leaves nothing of a longer one; a line wider than the box ends at the
 * last whole letter that leaves room for an ellipsis. On a panel mounted
 * turned (the Pixel 2's, 480 x 640), a landscape pixel (x, y) is the
 * buffer's column y of row W - 1 - x, as ZyperOS's plat_save_screen turns
 * the screen there.
 */
#ifndef SPLASH_PORTOUT_H
#define SPLASH_PORTOUT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PORTOUT_MAGIC   "ZPOU"
#define PORTOUT_VERSION 1
#define PORTOUT_HEADER  26
#define PORTOUT_GLYPHS  512     /* the most letters a file may have */
#define PORTOUT_CELL    128     /* the widest cell, and the tallest box */
#define PORTOUT_CHARS   256     /* the most letters a line keeps; more are dropped */
#define PORTOUT_ELLIPSIS 0x2026

typedef struct {
	uint32_t cp;
	unsigned adv, cw;
	int cx;
	const uint8_t *cov;     /* cw x h */
} portout_glyph;

typedef struct {
	unsigned land_w, land_h;   /* the landscape screen, panel pixels */
	unsigned x, y, w, h;       /* the line's box in it */
	uint32_t ink;              /* 0xRRGGBB */
	unsigned count;
	portout_glyph g[PORTOUT_GLYPHS];
	const portout_glyph *latin[256];   /* code points under 256, looked up at once */
} portout_atlas;

static unsigned portout_u16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* The file, `n` bytes at `p`, read into `A`, which points into `p`: 0, or
 * -1 for a file that is not one or does not hold together */
static int portout_load(const uint8_t *p, size_t n, portout_atlas *A)
{
	size_t at = PORTOUT_HEADER;

	memset(A, 0, sizeof *A);
	if (n < PORTOUT_HEADER || memcmp(p, PORTOUT_MAGIC, 4) || portout_u16(p + 4) != PORTOUT_VERSION)
		return -1;
	A->land_w = portout_u16(p + 6);
	A->land_h = portout_u16(p + 8);
	A->x = portout_u16(p + 10);
	A->y = portout_u16(p + 12);
	A->w = portout_u16(p + 14);
	A->h = portout_u16(p + 16);
	A->ink = (uint32_t)p[18] << 16 | (uint32_t)p[19] << 8 | p[20];
	A->count = portout_u16(p + 24);
	if (!A->w || !A->h || A->h > PORTOUT_CELL || A->x + A->w > A->land_w ||
	    A->y + A->h > A->land_h || A->count > PORTOUT_GLYPHS)
		return -1;
	for (unsigned i = 0; i < A->count; i++) {
		portout_glyph *g = &A->g[i];

		if (at + 10 > n) return -1;
		g->cp = p[at] | p[at + 1] << 8 | (uint32_t)p[at + 2] << 16 | (uint32_t)p[at + 3] << 24;
		g->adv = portout_u16(p + at + 4);
		g->cx = (int16_t)portout_u16(p + at + 6);
		g->cw = portout_u16(p + at + 8);
		at += 10;
		if (g->cw > PORTOUT_CELL || g->adv > PORTOUT_CELL || g->cx < -PORTOUT_CELL ||
		    at + (size_t)g->cw * A->h > n)
			return -1;
		g->cov = p + at;
		at += (size_t)g->cw * A->h;
		if (g->cp < 256) A->latin[g->cp] = g;
	}
	return 0;
}

static const portout_glyph *portout_glyph_of(const portout_atlas *A, uint32_t cp)
{
	if (cp < 256) return A->latin[cp];
	for (unsigned i = 0; i < A->count; i++)
		if (A->g[i].cp == cp) return &A->g[i];
	return NULL;
}

/* The line as a terminal shows it, read a byte at a time */
typedef struct {
	uint32_t cur[PORTOUT_CHARS];   /* the line being written */
	size_t n;
	uint32_t last[PORTOUT_CHARS];  /* the last whole line worth showing */
	size_t last_n;
	int cr;                        /* a \r came: the next letter starts the line again */
	int esc;                       /* 0, or in ESC, CSI, OSC, OSC's ESC */
	unsigned need;                 /* UTF-8 bytes still to come */
	uint32_t cp;
} portout_reader;

enum { PORTOUT_ESC = 1, PORTOUT_CSI, PORTOUT_OSC, PORTOUT_OSC_ESC };

/* ZyperOS's own programs, whose lines in the log are not the port's */
static int portout_ours(const uint32_t *s, size_t n)
{
	static const char *const own[] = { "portkeys: ", "zyhome: ", "port: ", "kernel: " };

	for (size_t k = 0; k < sizeof own / sizeof own[0]; k++) {
		size_t len = strlen(own[k]), i;

		if (n < len) continue;
		for (i = 0; i < len && s[i] == (unsigned char)own[k][i]; i++) {}
		if (i == len) return 1;
	}
	return 0;
}

/* `s`, `n` letters, as shown: the letters the font has, a run of spaces
 * one space, the ends trimmed; into `out`, how many */
static size_t portout_clean(const portout_atlas *A, const uint32_t *s, size_t n, uint32_t *out)
{
	size_t k = 0;

	for (size_t i = 0; i < n; i++) {
		uint32_t c = s[i] == 0xA0 ? ' ' : s[i];

		if (c == ' ') {
			if (k && out[k - 1] != ' ') out[k++] = ' ';
		} else if (portout_glyph_of(A, c)) {
			out[k++] = c;
		}
	}
	while (k && out[k - 1] == ' ') k--;
	return k;
}

/* A line ended: the last worth showing when it has a letter to show */
static void portout_line_end(portout_reader *R, const portout_atlas *A)
{
	uint32_t shown[PORTOUT_CHARS];

	if (R->n && !portout_ours(R->cur, R->n) && portout_clean(A, R->cur, R->n, shown)) {
		memcpy(R->last, R->cur, R->n * sizeof R->cur[0]);
		R->last_n = R->n;
	}
	R->n = 0;
	R->cr = 0;
}

static void portout_put(portout_reader *R, uint32_t c)
{
	if (R->cr) {               /* after a \r, the line again from its start */
		R->n = 0;
		R->cr = 0;
	}
	if (R->n < PORTOUT_CHARS) R->cur[R->n++] = c;
}

/* One code point of the stream */
static void portout_char(portout_reader *R, const portout_atlas *A, uint32_t c)
{
	/* A line end ends an escape left open, as a terminal still moves on
	 * one: a stray ESC ] would hold back every line after it */
	if (c == '\n' || c == '\r') R->esc = 0;
	switch (R->esc) {
	case PORTOUT_ESC:          /* intermediates (ESC ( B), then a final byte */
		if (c == '[') R->esc = PORTOUT_CSI;
		else if (c == ']') R->esc = PORTOUT_OSC;
		else if (c < 0x20 || c > 0x2F) R->esc = 0;
		return;
	case PORTOUT_CSI:          /* parameters, then a final byte */
		if (c >= 0x40 && c <= 0x7E) R->esc = 0;
		return;
	case PORTOUT_OSC:          /* until BEL, or ESC \ */
		if (c == 0x07) R->esc = 0;
		else if (c == 0x1B) R->esc = PORTOUT_OSC_ESC;
		return;
	case PORTOUT_OSC_ESC:
		R->esc = c == '\\' ? 0 : PORTOUT_OSC;
		return;
	}
	if (c == '\n') {
		portout_line_end(R, A);
	} else if (c == '\r') {
		R->cr = 1;
	} else if (c == 0x1B) {
		R->esc = PORTOUT_ESC;
	} else if (c == '\b') {
		if (R->cr) { R->n = 0; R->cr = 0; }
		if (R->n) R->n--;
	} else if (c == '\t') {
		portout_put(R, ' ');
	} else if (c >= 0x20 && c != 0x7F && (c < 0x80 || c > 0x9F)) {
		portout_put(R, c);
	}
}

/* `n` bytes of the log, in order; `A` says which letters can be shown */
static void portout_feed(portout_reader *R, const portout_atlas *A, const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		uint8_t b = p[i];

		if (R->need) {
			if ((b & 0xC0) == 0x80) {
				R->cp = R->cp << 6 | (b & 0x3F);
				if (--R->need == 0) portout_char(R, A, R->cp);
				continue;
			}
			R->need = 0;       /* cut short: dropped, and this byte read anew */
		}
		if (b < 0x80) portout_char(R, A, b);
		else if ((b & 0xE0) == 0xC0) { R->cp = b & 0x1F; R->need = 1; }
		else if ((b & 0xF0) == 0xE0) { R->cp = b & 0x0F; R->need = 2; }
		else if ((b & 0xF8) == 0xF0) { R->cp = b & 0x07; R->need = 3; }
	}
}

/* What a fresh start of reading forgets: a line half read, an escape half
 * read. The last whole line stays. */
static void portout_skip(portout_reader *R)
{
	R->n = 0;
	R->cr = 0;
	R->esc = 0;
	R->need = 0;
}

/* The line to show, cleaned (portout_clean), into `out`: how many letters,
 * 0 for none yet */
static size_t portout_shown(const portout_reader *R, const portout_atlas *A, uint32_t *out)
{
	size_t k = 0;

	if (R->n && !portout_ours(R->cur, R->n)) k = portout_clean(A, R->cur, R->n, out);
	if (!k) k = portout_clean(A, R->last, R->last_n, out);
	return k;
}

/* The box: `under`, the screen's own pixels of it (w x h, 0xXXRRGGBB, its
 * rows landscape), with `s` (`n` letters, cleaned) laid over it in the ink,
 * into `px` (w x h). How many of the letters were drawn. */
static size_t portout_compose(const portout_atlas *A, const uint32_t *under, const uint32_t *s,
                              size_t n, uint32_t *px)
{
	const portout_glyph *dots = portout_glyph_of(A, PORTOUT_ELLIPSIS);
	unsigned total = 0, pen = 0, room;
	size_t shown = n, i;

	memcpy(px, under, (size_t)A->w * A->h * 4);
	for (i = 0; i < n; i++) total += portout_glyph_of(A, s[i])->adv;
	if (total > A->w) {
		/* The most whole letters that leave room for the ellipsis, and no
		 * space left before it */
		room = dots && dots->adv < A->w ? A->w - dots->adv : A->w;
		for (shown = 0, total = 0; shown < n; shown++) {
			unsigned a = portout_glyph_of(A, s[shown])->adv;

			if (total + a > room) break;
			total += a;
		}
		while (shown && s[shown - 1] == ' ') shown--;
	}
	for (i = 0; i <= shown; i++) {
		const portout_glyph *g = i < shown ? portout_glyph_of(A, s[i]) : (shown < n ? dots : NULL);
		uint32_t ir = A->ink >> 16 & 0xff, ig = A->ink >> 8 & 0xff, ib = A->ink & 0xff;

		if (!g) break;
		for (unsigned y = 0; y < A->h; y++)
			for (unsigned x = 0; x < g->cw; x++) {
				int bx = (int)pen + g->cx + (int)x;
				uint32_t a = g->cov[(size_t)y * g->cw + x], d, *q;

				if (!a || bx < 0 || bx >= (int)A->w) continue;
				q = &px[(size_t)y * A->w + (unsigned)bx];
				d = *q;
				*q = 0xff000000u |
				     (((d >> 16 & 0xff) * (255 - a) + ir * a) / 255) << 16 |
				     (((d >> 8 & 0xff) * (255 - a) + ig * a) / 255) << 8 |
				     (((d & 0xff) * (255 - a) + ib * a) / 255);
			}
		pen += g->adv;
	}
	return shown;
}

/* `px` (w x h, from portout_compose) into the box of `buf`, the panel's
 * buffer, `pitch` pixels a row: landscape as it is, or turned (see the top) */
static void portout_put_box(const portout_atlas *A, const uint32_t *px, uint32_t *buf, size_t pitch,
                            int turned)
{
	if (!turned) {
		for (unsigned y = 0; y < A->h; y++)
			memcpy(buf + (size_t)(A->y + y) * pitch + A->x, px + (size_t)y * A->w, (size_t)A->w * 4);
		return;
	}
	/* A landscape column is a run of a row of the turned buffer: written a
	 * run at a time, as a buffer the display reads is uncached */
	for (unsigned x = 0; x < A->w; x++) {
		uint32_t col[PORTOUT_CELL];
		uint32_t *row = buf + (size_t)(A->land_w - 1 - (A->x + x)) * pitch + A->y;

		for (unsigned y = 0; y < A->h; y++) col[y] = px[(size_t)y * A->w + x];
		memcpy(row, col, (size_t)A->h * 4);
	}
}

/* The box's own pixels, from `buf` as portout_put_box writes them, into
 * `under` (w x h) */
static void portout_get_box(const portout_atlas *A, const uint32_t *buf, size_t pitch, int turned,
                            uint32_t *under)
{
	for (unsigned y = 0; y < A->h; y++)
		for (unsigned x = 0; x < A->w; x++)
			under[(size_t)y * A->w + x] = turned
				? buf[(size_t)(A->land_w - 1 - (A->x + x)) * pitch + A->y + y]
				: buf[(size_t)(A->y + y) * pitch + A->x + x];
}

#endif
