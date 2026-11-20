/* SPDX-License-Identifier: GPL-3.0-only */
/* See rahash.h. Three pieces: MD5, enough zip to reach the ROM, and
 * RetroAchievements' per-console rule about how much of it counts. */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <zlib.h>
#include <fcntl.h>

#include "discimg.h"
#include "rahash.h"

/* ---------------------------------------------------------------- MD5 ---- */

typedef struct {
	uint32_t a, b, c, d;
	uint64_t len;
	unsigned char buf[64];
	size_t have;
} md5_ctx;

static const uint32_t K[64] = {
	0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,
	0xa8304613,0xfd469501,0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
	0x6b901122,0xfd987193,0xa679438e,0x49b40821,0xf61e2562,0xc040b340,
	0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
	0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,
	0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
	0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,0x289b7ec6,0xeaa127fa,
	0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
	0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,
	0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
	0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static const unsigned char S[64] = {
	7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
	5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
	4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
	6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
};

static uint32_t rol(uint32_t x, int c) { return (x << c) | (x >> (32 - c)); }

static void md5_block(md5_ctx *m, const unsigned char *p)
{
	uint32_t M[16], a = m->a, b = m->b, c = m->c, d = m->d;
	int i;

	for (i = 0; i < 16; i++)
		M[i] = (uint32_t)p[i*4] | ((uint32_t)p[i*4+1] << 8) |
		       ((uint32_t)p[i*4+2] << 16) | ((uint32_t)p[i*4+3] << 24);

	for (i = 0; i < 64; i++) {
		uint32_t f;
		int g;

		if (i < 16)      { f = (b & c) | (~b & d);        g = i; }
		else if (i < 32) { f = (d & b) | (~d & c);        g = (5*i + 1) & 15; }
		else if (i < 48) { f = b ^ c ^ d;                 g = (3*i + 5) & 15; }
		else             { f = c ^ (b | ~d);              g = (7*i) & 15; }

		f += a + K[i] + M[g];
		a = d; d = c; c = b;
		b += rol(f, S[i]);
	}
	m->a += a; m->b += b; m->c += c; m->d += d;
}

static void md5_init(md5_ctx *m)
{
	m->a = 0x67452301; m->b = 0xefcdab89;
	m->c = 0x98badcfe; m->d = 0x10325476;
	m->len = 0; m->have = 0;
}

static void md5_update(md5_ctx *m, const void *data, size_t n)
{
	const unsigned char *p = data;

	m->len += n;
	while (n) {
		size_t take = 64 - m->have;
		if (take > n) take = n;
		memcpy(m->buf + m->have, p, take);
		m->have += take; p += take; n -= take;
		if (m->have == 64) { md5_block(m, m->buf); m->have = 0; }
	}
}

static void md5_hex(md5_ctx *m, char *out)
{
	uint64_t bits = m->len * 8;
	unsigned char tail[8];
	static const unsigned char pad[64] = { 0x80 };
	size_t padn;
	uint32_t w[4];
	int i;

	padn = (m->have < 56) ? 56 - m->have : 120 - m->have;
	md5_update(m, pad, padn);
	for (i = 0; i < 8; i++) tail[i] = (unsigned char)(bits >> (8 * i));
	md5_update(m, tail, 8);

	w[0] = m->a; w[1] = m->b; w[2] = m->c; w[3] = m->d;
	for (i = 0; i < 16; i++)
		sprintf(out + i * 2, "%02x", (unsigned)((w[i / 4] >> (8 * (i % 4))) & 0xff));
	out[32] = '\0';
}

/* ------------------------------------------------------------ the zip ---- */

static uint16_t rd16(const unsigned char *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* The disc containers ZyperOS's shelves take: src/library.c's list, less .bin,
 * which is a cartridge too, and the Dreamcast's .cdi and .gdi, which that list
 * has no folder to rank. .cso and .mds were missing until 2026-11-14, so a
 * PSP .cso, up to 1.8 GB, was read whole into memory beside the game it was
 * running, for a hash of the container that no game has; a .cdi was the same,
 * up to 700 MB. */
static bool is_disc_image(const char *path)
{
	static const char *const ext[] = {
		"chd", "cue", "ccd", "toc", "iso", "cso", "img", "m3u", "pbp", "mdf",
		"cbn", "mds", "cdi", "gdi", NULL
	};
	const char *dot = strrchr(path, '.'), *slash = strrchr(path, '/');
	int i;

	if (!dot || (slash && dot < slash)) return false;
	for (i = 0; ext[i]; i++)
		if (!strcasecmp(dot + 1, ext[i])) return true;
	return false;
}

static bool is_zip(const char *path)
{
	size_t n = path ? strlen(path) : 0;
	return n > 4 && strcasecmp(path + n - 4, ".zip") == 0;
}

/* The largest entry, whole. Deliberately the same choice Diatom's zip.c
 * makes - see rahash.h for why that coupling matters. */
static unsigned char *zip_largest(const char *path, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	unsigned char tail[66000 + 22], hdr[46], lh[30];
	long fsize, tail_off;
	size_t tail_len, i;
	uint32_t cd_off = 0, cd_n = 0;
	uint32_t best_u = 0, best_c = 0, best_lho = 0;
	uint16_t best_m = 0;
	unsigned char *comp = NULL, *un = NULL;
	int found = 0;

	*out_len = 0;
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	fsize = ftell(f);
	tail_len = fsize < (long)sizeof tail ? (size_t)fsize : sizeof tail;
	if (tail_len < 22) { fclose(f); return NULL; }
	tail_off = fsize - (long)tail_len;
	fseek(f, tail_off, SEEK_SET);
	if (fread(tail, 1, tail_len, f) != tail_len) { fclose(f); return NULL; }
	for (i = tail_len - 22 + 1; i-- > 0; ) {
		if (tail[i] == 'P' && tail[i+1] == 'K' && tail[i+2] == 5 && tail[i+3] == 6) {
			cd_n   = rd16(tail + i + 10);
			cd_off = rd32(tail + i + 16);
			found = 1;
			break;
		}
	}
	if (!found || !cd_n) { fclose(f); return NULL; }

	fseek(f, (long)cd_off, SEEK_SET);
	for (i = 0; i < cd_n; i++) {
		char en[512];
		uint16_t nlen, elen, clen, method;
		uint32_t usize;

		if (fread(hdr, 1, 46, f) != 46 || rd32(hdr) != 0x02014b50) break;
		method = rd16(hdr + 10);
		usize  = rd32(hdr + 24);
		nlen   = rd16(hdr + 28);
		elen   = rd16(hdr + 30);
		clen   = rd16(hdr + 32);
		if (nlen >= sizeof en) break;
		if (fread(en, 1, nlen, f) != nlen) break;
		en[nlen] = '\0';
		fseek(f, elen + clen, SEEK_CUR);

		if (usize == 0 || (nlen && en[nlen - 1] == '/')) continue;
		if (method != 0 && method != 8) continue;
		if (usize > best_u) {
			best_u = usize;
			best_c = rd32(hdr + 20);
			best_m = method;
			best_lho = rd32(hdr + 42);
		}
	}
	if (!best_u) { fclose(f); return NULL; }

	fseek(f, (long)best_lho, SEEK_SET);
	if (fread(lh, 1, 30, f) != 30 || rd32(lh) != 0x04034b50) { fclose(f); return NULL; }
	fseek(f, rd16(lh + 26) + rd16(lh + 28), SEEK_CUR);

	comp = malloc(best_c ? best_c : 1);
	un   = malloc(best_u);
	if (!comp || !un) goto fail;
	if (fread(comp, 1, best_c, f) != best_c) goto fail;
	fclose(f);
	f = NULL;

	if (best_m == 0) {
		memcpy(un, comp, best_u);
	} else {
		z_stream z;
		int rc;

		memset(&z, 0, sizeof z);
		z.next_in = comp; z.avail_in = best_c;
		z.next_out = un;  z.avail_out = best_u;
		/* -15: raw deflate, no zlib wrapper, which is what a zip stores. */
		if (inflateInit2(&z, -15) != Z_OK) goto fail;
		rc = inflate(&z, Z_FINISH);
		inflateEnd(&z);
		if (rc != Z_STREAM_END || z.total_out != best_u) goto fail;
	}
	free(comp);
	*out_len = best_u;
	return un;

fail:
	if (f) fclose(f);
	free(comp);
	free(un);
	return NULL;
}

/* --------------------------------------------------------- the rules ----- */

/* How much of a ROM RetroAchievements actually hashes. Theirs, not ours:
 *
 *   NES         an iNES header is 16 bytes of container, not game
 *   SFC         copier headers are 512 bytes and only sometimes present,
 *               which is what the size test detects
 *   PCE         the same idea against a different block size
 *   MD/SMS/GG   an SMD-style header, likewise conditional
 *   GB/GBC/GBA  the whole file; there is no container to strip
 *   NES (FDS)   fwNES's 16-byte "FDS\x1a" header on a Famicom Disk System
 *               image, rc_hash_nes's second case: FDS games are on the NES
 *               shelf, and hashed whole they matched nothing
 *   A7800       a 128-byte A78 header, "ATARI7800" at byte 1 (rc_hash_7800)
 *   LYNX        a 64-byte "LYNX\0" header (rc_hash_lynx)
 */
static size_t ra_skip(const char *tag, const unsigned char *d, size_t n)
{
	if (!strcmp(tag, "NES")) {
		if (n >= 4 && !memcmp(d, "NES\x1a", 4)) return 16;
		if (n >= 4 && !memcmp(d, "FDS\x1a", 4)) return 16;
		return 0;
	}
	if (!strcmp(tag, "A7800"))
		return (n >= 10 && !memcmp(d + 1, "ATARI7800", 9)) ? 128 : 0;
	if (!strcmp(tag, "LYNX"))
		return (n >= 5 && !memcmp(d, "LYNX", 5)) ? 64 : 0;
	if (!strcmp(tag, "SFC"))
		return (n % 1024 == 512) ? 512 : 0;
	if (!strcmp(tag, "PCE"))
		return (n % 131072 == 512) ? 512 : 0;
	if (!strcmp(tag, "MD") || !strcmp(tag, "SMS") || !strcmp(tag, "GG"))
		return (n % 16384 == 512) ? 512 : 0;
	return 0;
}

/* A Nintendo 64 ROM is hashed in its big-endian (.z64) order, whichever of
 * the three orders the file is in: rcheevos's rc_hash_n64, which reads the
 * first byte - 0x80 big-endian, 0x37 byte-swapped (.v64), 0x40 little-endian
 * words (.n64) - and turns the rest round to match. Anything else is not an
 * N64 ROM to RetroAchievements, and gets no hash. In place. */
static bool n64_to_z64(unsigned char *d, size_t n)
{
	size_t i;

	if (!n) return false;
	if (d[0] == 0x80) return true;
	if (d[0] == 0x37) {
		for (i = 0; i + 1 < n; i += 2) {
			unsigned char t = d[i];

			d[i] = d[i + 1];
			d[i + 1] = t;
		}
		return true;
	}
	if (d[0] == 0x40) {
		for (i = 0; i + 3 < n; i += 4) {
			unsigned char t0 = d[i], t1 = d[i + 1];

			d[i] = d[i + 3];
			d[i + 1] = d[i + 2];
			d[i + 2] = t1;
			d[i + 3] = t0;
		}
		return true;
	}
	return false;
}

/* ---------------------------------------------------- PlayStation ---- */

/* A PlayStation disc is not hashed whole. RetroAchievements hashes the boot
 * executable SYSTEM.CNF names (or PSX.EXE when there is none), prefixed with
 * that executable's own name: md5(name + file). Theirs, restated from
 * rcheevos's rc_hash_psx and its ISO 9660 walk, rc_hash_disc.c, at 0aea57b.
 * The size is the PS-X EXE header's text size plus the 2048-byte header, not
 * the directory's, when the file has one.
 *
 * Read sector by sector, never the whole image: a disc is up to 700 MB and the
 * Pixel 2 has 1 GB, so the cartridge path's "read it all, then hash" would
 * fail or, worse, push the running game out of memory.
 *
 * WHAT THIS READS: .cue (its first FILE, which holds track 1), a bare .bin,
 * .iso or .img, .m3u (its first disc, as rcheevos does), and, through
 * src/discimg.c, a .chd (its first track) and a PSone Classics .pbp (its
 * first disc). For those two RetroAchievements has no rule of its own: the
 * hash is the disc's, whatever holds it, which is what RetroArch's CHD reader
 * gives rcheevos and what DuckStation, which reads a .pbp, hashes too. So a
 * game converted from .cue/.bin to .chd or .pbp keeps its hash, which RA
 * knows, rather than one of the container, which it would not.
 *
 * A PlayStation .pbp read with no system known (RetroArch's way: rcheevos
 * files the extension under the PSP) is hashed whole, and RA knows no
 * PlayStation game by that. ZyperOS always knows the shelf, so it never asks
 * that way. */
typedef struct {
	disc *f;
	long  ss;        /* bytes a sector takes in the file: 2352, 2336 or 2048 */
	long  hdr;       /* bytes before the 2048 of data: 24, 16, 8 or 0 */
	long  first;     /* the disc sector the file's first sector is */
	long  offset;    /* where track 1 starts in the file */
	long  count;     /* the track's sectors, where it ends before the file
	                  * does (a Dreamcast .cdi's); 0 reads to the end */
} psx_track;

static uint32_t rd24(const unsigned char *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static int bcd(unsigned char b) { return (b >> 4) * 10 + (b & 15); }

static size_t psx_read(psx_track *t, long sector, void *buf, size_t n)
{
	if (sector < t->first) return 0;
	if (t->count && sector - t->first >= t->count) return 0;
	if (n > 2048) n = 2048;
	return disc_read(t->f, (uint64_t)(sector - t->first) * (uint64_t)t->ss +
	                 (uint64_t)t->hdr + (uint64_t)t->offset, buf, n);
}

/* rcheevos's cdreader_determine_sector_size: look for the sync pattern at
 * sector 16 under each sector size, and for "CD001" in the volume descriptor
 * that is always there. */
static bool psx_sense(psx_track *t)
{
	static const unsigned char sync[12] = {
		0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
	};
	static const long sizes[2] = { 2352, 2336 };
	unsigned char h[32];
	int i;

	for (i = 0; i < 2; i++) {
		if (disc_read(t->f, (uint64_t)16 * (uint64_t)sizes[i] + (uint64_t)t->offset,
		              h, sizeof h) != sizeof h)
			continue;
		if (memcmp(h, sync, 12)) continue;
		t->ss  = sizes[i];
		t->hdr = memcmp(h + 25, "CD001", 5) ? 16 : 24;
		t->first = (bcd(h[12]) * 60 + bcd(h[13])) * 75 + bcd(h[14]) - 150 - 16;
		return true;
	}
	if (disc_read(t->f, (uint64_t)16 * 2048 + (uint64_t)t->offset, h, sizeof h) == sizeof h &&
	    !memcmp(h + 1, "CD001", 5)) {
		t->ss = 2048;
		t->hdr = 0;
		t->first = 0;
		return true;
	}
	return false;
}

static bool has_ext(const char *path, const char *ext)
{
	const char *dot = strrchr(path, '.');
	return dot && !strcasecmp(dot + 1, ext);
}

/* `path` with its last component replaced by `name`. */
static void sibling(const char *path, const char *name, char *out, size_t n)
{
	const char *slash = strrchr(path, '/');

	if (name[0] == '/' || !slash) snprintf(out, n, "%s", name);
	else snprintf(out, n, "%.*s/%s", (int)(slash - path), path, name);
}

/* The first line of an .m3u that is not blank and not a comment. */
static bool m3u_first(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");
	char line[1024];

	if (!f) return false;
	while (fgets(line, sizeof line, f)) {
		char *e = line + strcspn(line, "\r\n");
		char *b = line;

		*e = '\0';
		while (*b == ' ' || *b == '\t') b++;
		if (!*b || *b == '#') continue;
		/* rcheevos's m3u reader keeps a "|label" suffix out of the name */
		b[strcspn(b, "|")] = '\0';
		sibling(path, b, out, n);
		fclose(f);
		return true;
	}
	fclose(f);
	return false;
}

/* Track 1 of a .cue: the file it lives in and where it starts in it. */
static bool cue_track1(const char *path, char *file, size_t n, long *offset)
{
	FILE *f = fopen(path, "r");
	char line[1024], cur[1024] = "";
	long ss = 2352;
	bool in1 = false;

	if (!f) return false;
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		int mm, s, fr;

		while (*p == ' ' || *p == '\t') p++;
		if (!strncasecmp(p, "FILE", 4)) {
			char *q = strchr(p, '"'), *r = q ? strchr(q + 1, '"') : NULL;
			if (q && r) snprintf(cur, sizeof cur, "%.*s", (int)(r - q - 1), q + 1);
		} else if (!strncasecmp(p, "TRACK", 5)) {
			in1 = atoi(p + 5) == 1;
			if (in1) ss = strstr(p, "/2048") ? 2048 : strstr(p, "/2336") ? 2336 : 2352;
		} else if (in1 && !strncasecmp(p, "INDEX 01", 8) &&
		           sscanf(p + 8, "%d:%d:%d", &mm, &s, &fr) == 3 && cur[0]) {
			sibling(path, cur, file, n);
			*offset = ((long)mm * 60 + s) * 75 * ss + (long)fr * ss;
			fclose(f);
			return true;
		}
	}
	fclose(f);
	return false;
}

/* rcheevos's rc_cd_find_file_sector: walk from the root through any
 * backslash-separated directories, then match the name with or without its
 * ";1" version. Returns the file's sector, 0 when it is not there. */
static uint32_t psx_find(psx_track *t, const char *path, uint32_t *size)
{
	unsigned char pvd[256], sec[2048];
	uint32_t lbs, sector, remaining;
	const char *slash;
	size_t len;

	if (psx_read(t, t->first + 16, pvd, sizeof pvd) != sizeof pvd ||
	    memcmp(pvd, "\1CD001", 6))
		return 0;
	lbs = rd16(pvd + 128);
	sector = rd24(pvd + 156 + 2);
	remaining = rd32(pvd + 156 + 10);
	if (!lbs) return 0;

	if (*path == '\\') path++;
	for (;;) {
		bool dir;
		const unsigned char *r;
		bool found = false;

		slash = strchr(path, '\\');
		dir = slash != NULL;
		len = dir ? (size_t)(slash - path) : strlen(path);

		for (;;) {
			if (!psx_read(t, sector, sec, sizeof sec)) return 0;
			for (r = sec; r < sec + sizeof sec - 34 && *r; r += *r) {
				const char *name = (const char *)r + 33;
				unsigned nlen = r[32];

				/* The image is untrusted input. A record that says its
				 * name is longer than the record, or than the sector,
				 * ends the scan: comparing it would read past `sec`.
				 * rcheevos compares first and checks after, which on a
				 * real disc never differs, and on a crafted one is the
				 * over-read this refuses. Only a name at least as long
				 * as the one wanted is compared, and the byte after it
				 * is read only when the name has one. */
				if (r + *r > sec + sizeof sec || 33u + nlen > *r ||
				    r + 33 + nlen > sec + sizeof sec)
					break;
				if (nlen >= len && !strncasecmp(name, path, len) &&
				    (nlen == len || (!dir && name[len] == ';'))) {
					if (dir) {
						/* Only the sector: rcheevos keeps counting
						 * down the ROOT's length inside a
						 * subdirectory, and the hash has to read
						 * what it reads. */
						sector = rd24(r + 2);
					} else {
						if (size) *size = rd32(r + 10);
						return rd24(r + 2);
					}
					found = true;
					break;
				}
			}
			if (found || remaining <= lbs) break;
			remaining -= lbs;
			sector++;
		}
		if (!found) return 0;
		path = slash + 1;
	}
}

static bool ra_hash_psx(const char *path, char *out)
{
	char file[1024], name[64] = "", m3u[1024];
	unsigned char buf[2048];
	psx_track t = { 0 };
	uint32_t sector = 0, size = 0;
	size_t got;
	md5_ctx m;

	if (has_ext(path, "m3u")) {
		if (!m3u_first(path, m3u, sizeof m3u) || has_ext(m3u, "m3u")) return false;
		path = m3u;
	}
	if (has_ext(path, "cue")) {
		if (!cue_track1(path, file, sizeof file, &t.offset)) return false;
		t.f = disc_open_file(file);
	} else if (has_ext(path, "bin") || has_ext(path, "iso") || has_ext(path, "img")) {
		t.f = disc_open_file(path);
	} else if (has_ext(path, "chd")) {
		t.f = disc_open_chd_cd(path);
	} else if (has_ext(path, "pbp")) {
		t.f = disc_open_pbp_psx(path);
	} else {
		return false;
	}
	if (!t.f) return false;
	if (!psx_sense(&t)) { disc_close(t.f); return false; }

	/* SYSTEM.CNF's BOOT line, exactly as rc_hash_find_playstation_executable
	 * reads it: the first sector only, "cdrom:" and leading backslashes off,
	 * up to whitespace or the ";1". */
	sector = psx_find(&t, "SYSTEM.CNF", NULL);
	if (sector) {
		char *p, *start;

		got = psx_read(&t, sector, buf, sizeof buf - 1);
		buf[got] = '\0';
		sector = 0;
		for (p = (char *)buf; *p; p++) {
			if (!strncmp(p, "BOOT", 4)) {
				p += 4;
				while (isspace((unsigned char)*p)) p++;
				if (*p == '=') {
					size_t n;

					p++;
					while (isspace((unsigned char)*p)) p++;
					if (!strncmp(p, "cdrom:", 6)) p += 6;
					while (*p == '\\') p++;
					start = p;
					while (*p && !isspace((unsigned char)*p) && *p != ';') p++;
					n = (size_t)(p - start);
					if (n >= sizeof name) n = sizeof name - 1;
					memcpy(name, start, n);
					name[n] = '\0';
					sector = psx_find(&t, name, &size);
					break;
				}
			}
			while (*p && *p != '\n') p++;
			if (!*p) break;
		}
	}
	if (!sector) {
		sector = psx_find(&t, "PSX.EXE", &size);
		if (sector) snprintf(name, sizeof name, "PSX.EXE");
	}
	if (!sector || psx_read(&t, sector, buf, 32) < 32) { disc_close(t.f); return false; }
	if (!memcmp(buf, "PS-X EXE", 7))
		size = rd32(buf + 28) + 2048;
	if (size > 64u * 1024 * 1024) size = 64u * 1024 * 1024;

	md5_init(&m);
	md5_update(&m, name, strlen(name));
	while (size > 0) {
		size_t want = size < sizeof buf ? size : sizeof buf;

		got = psx_read(&t, sector++, buf, want);
		if (!got) break;
		md5_update(&m, buf, got);
		size -= (uint32_t)got;
	}
	disc_close(t.f);
	md5_hex(&m, out);
	return true;
}

/* ------------------------------------------------------------- PSP ---- */

/* A PSP disc is hashed by two of its files, one after the other, with no
 * names: PSP_GAME/PARAM.SFO (the game's title and serial) and
 * PSP_GAME/SYSDIR/EBOOT.BIN (its executable), rcheevos's rc_hash_psp, each
 * as long as its directory says, the second capped at 64 MB as theirs is.
 * The walk is the PlayStation's (psx_find), over 2048-byte sectors.
 *
 * .iso is read as it is, .cso and .chd as the ISO they hold, the way PPSSPP
 * reads them (src/discimg.c): its own RetroAchievements client hashes what
 * its block device gives it, so the hash is the ISO's. A .pbp - a PSN game's
 * EBOOT.PBP - is hashed whole, rc_hash_psp's first rule: it holds the
 * PARAM.SFO and the executable and more besides, and RA hashes all of it.
 * That is the one hash here that reads a file to its end; up to 1.8 GB, it
 * goes through 64 KB at a time and, on Linux, tells the kernel to drop what
 * it has read, so the game's own reads keep the page cache. */

/* rcheevos's rc_hash_cd_file: `size` bytes of the file at `sector`, capped
 * at 64 MB, its first sector whole or not at all */
static bool cd_add_file(psx_track *t, md5_ctx *m, uint32_t sector, uint32_t size)
{
	unsigned char buf[2048];
	size_t got;

	if (!sector) return false;
	/* the file's first sector must read whole */
	if (psx_read(t, (long)sector, buf, sizeof buf) < sizeof buf) return false;
	if (size > 64u * 1024 * 1024) size = 64u * 1024 * 1024;
	got = sizeof buf;
	for (;;) {
		size_t take = size < got ? size : got;

		md5_update(m, buf, take);
		if (size <= got) break;
		size -= (uint32_t)got;
		got = psx_read(t, (long)++sector, buf, size < sizeof buf ? size : sizeof buf);
		if (!got) break;
	}
	return true;
}

static bool psp_add_file(psx_track *t, md5_ctx *m, const char *name)
{
	uint32_t size = 0, sector = psx_find(t, name, &size);

	return sector && cd_add_file(t, m, sector, size);
}

static bool hash_whole_file(const char *path, char *out)
{
	/* Not static: the flush worker can hash while the main thread does */
	unsigned char *buf = malloc(64 * 1024);
	FILE *f = buf ? fopen(path, "rb") : NULL;
	md5_ctx m;
	size_t got;
	off_t done = 0;

	if (!f) { free(buf); return false; }
	md5_init(&m);
	while ((got = fread(buf, 1, 64 * 1024, f)) > 0) {
		md5_update(&m, buf, got);
		done += (off_t)got;
#if defined(__linux__) && defined(POSIX_FADV_DONTNEED)
		posix_fadvise(fileno(f), 0, done, POSIX_FADV_DONTNEED);
#endif
	}
	free(buf);
	if (ferror(f) || !done) { fclose(f); return false; }
	fclose(f);
	md5_hex(&m, out);
	return true;
}

static bool ra_hash_psp(const char *path, char *out)
{
	psx_track t = { 0 };
	md5_ctx m;
	bool ok;

	if (has_ext(path, "pbp")) return hash_whole_file(path, out);
	if (has_ext(path, "iso")) t.f = disc_open_file(path);
	else if (has_ext(path, "cso")) t.f = disc_open_cso(path);
	else if (has_ext(path, "chd")) t.f = disc_open_chd_2048(path);
	if (!t.f) return false;
	if (!psx_sense(&t)) { disc_close(t.f); return false; }
	md5_init(&m);
	ok = psp_add_file(&t, &m, "PSP_GAME\\PARAM.SFO") &&
	     psp_add_file(&t, &m, "PSP_GAME\\SYSDIR\\EBOOT.BIN");
	disc_close(t.f);
	if (ok) md5_hex(&m, out);
	return ok;
}

/* ------------------------------------------------------- Dreamcast ---- */

/* A Dreamcast disc is hashed by its IP.BIN and its boot file: rcheevos's
 * rc_hash_dreamcast (hash_disc.c, f1417fb). The md5 of the first 256 bytes
 * of track 3 - the GD-ROM's high-density area, which starts with IP.BIN's
 * "SEGA SEGAKATANA " - or, where track 3 is not that (a MIL-CD, as a .cdi
 * usually is), of the first data track's; then of the file IP.BIN names at
 * byte 96 (1ST_READ.BIN on most games), as long as its directory says and
 * capped at 64 MB, read from that track, or from the disc's last track when
 * the file's first sector is past that track's end.
 *
 * The file system counts the disc's own sectors - a GD-ROM's track 3 is
 * sector 45000 - so what matters in each container is where it puts a track:
 *
 *   .gdi  the line's LBA: rcheevos's own reader, cdreader_open_gdi_track
 *   .cue  rcheevos's own reader too, cdreader_open_cue_track: the sheet's
 *         positions, then a raw sector's own header for where it is
 *   .chd  RetroArch's CHD reader (libretro-common's chdstream and cdfs):
 *         the frames of every track before it (chd_cd_track)
 *   .cdi  DiscJuggler's track list as Flycast reads it (imgread/cdi.cpp):
 *         the standalone Flycast's own RetroAchievements client hashes
 *         through that reader, and RetroArch reads no .cdi
 *
 * All four come to the disc's sectors, so a game hashes the same from any
 * of them. Until 2026-11-19 every Dreamcast disc was refused here unread
 * (refused_by_name), so no Dreamcast game ever had a set, on a first play or
 * from MENU > Achievements' Download: found on the 1.1.0 test card, whose
 * two .chd games and one .cdi all said "not for this system". */
#define DC_FIRST_DATA CHD_TRACK_FIRST_DATA
#define DC_LAST       CHD_TRACK_LAST

static const unsigned char cd_sync[12] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

/* 32 bytes of sector `sector`, `size` bytes a sector from `off` */
static bool dc_peek(disc *f, long off, long sector, long size, unsigned char *h)
{
	return disc_read(f, (uint64_t)off + (uint64_t)sector * (uint64_t)size, h, 32) == 32;
}

static long dc_msf(const unsigned char *h)
{
	return (bcd(h[12]) * 60 + bcd(h[13])) * 75 + bcd(h[14]) - 150;
}

/* Track `want` of a .gdi, rcheevos's cdreader_open_gdi_track: the first line
 * is the count, each one after it "<track> <lba> <type> <sector size>
 * <file> <offset>", the file in quotes when it has spaces; type 4 is data.
 * The sector size is the file's own (psx_sense, rcheevos's
 * cdreader_determine_sector_size), else the line's, as "MODE1/<size>". */
static bool dc_gdi_track(const char *path, int want, psx_track *t)
{
	FILE *f = fopen(path, "r");
	char line[1024], file[512] = "", bin[1024];
	long lba = 0, size = 0;
	bool found = false;

	if (!f) return false;
	if (!fgets(line, sizeof line, f)) { fclose(f); return false; }
	if (want == DC_LAST) want = atoi(line);
	while (!found && fgets(line, sizeof line, f)) {
		char *p = line, *q;
		long num, l, type, ss;

		num = strtol(p, &q, 10);
		if (q == p) continue;
		l = strtol(q, &p, 10);
		type = strtol(p, &q, 10);
		ss = strtol(q, &p, 10);
		while (isspace((unsigned char)*p)) p++;
		if (*p == '"') {
			q = strchr(++p, '"');
			if (!q) continue;
		} else {
			for (q = p; *q && !isspace((unsigned char)*q); q++) ;
		}
		if (want == DC_FIRST_DATA ? type != 4 : num != want) continue;
		snprintf(file, sizeof file, "%.*s", (int)(q - p), p);
		lba = l;
		size = ss;
		found = true;
	}
	fclose(f);
	if (!found || !file[0]) return false;
	sibling(path, file, bin, sizeof bin);
	if (!(t->f = disc_open_file(bin))) return false;
	if (!psx_sense(t)) {
		if (size == 2352) { t->ss = 2352; t->hdr = 16; }
		else if (size == 2048) { t->ss = 2048; t->hdr = 0; }
		else { disc_close(t->f); t->f = NULL; return false; }
	}
	t->first = lba;
	return true;
}

/* A track as a .cue sheet places it, in rcheevos's terms */
typedef struct {
	int id, first, pregap, count;
	long ss, file_off, file_first;
	bool data;
	char mode[16], file[512];
} dc_cue;

static long file_bytes(const char *path)
{
	FILE *f = fopen(path, "rb");
	long n = 0;

	if (f && fseek(f, 0, SEEK_END) == 0) n = ftell(f);
	if (f) fclose(f);
	return n > 0 ? n : 0;
}

/* Track `want` of a .cue, rcheevos's cdreader_open_cue_track: the FILE that
 * holds it, where in that file it starts (the file's tracks before it), its
 * pregap (INDEX 00 to 01) and the disc sector the sheet puts it at (the
 * files before it, each as long as it is). A raw track's own sector header
 * then says where it is, as rcheevos lets it. */
static bool dc_cue_track(const char *path, int want, psx_track *t)
{
	FILE *f = fopen(path, "r");
	char line[1024], bin[1024];
	unsigned char h[32];
	dc_cue cur, prev;
	bool done = false;
	long pregap, first, off;

	if (!f) return false;
	memset(&cur, 0, sizeof cur);
	memset(&prev, 0, sizeof prev);
	while (!done && fgets(line, sizeof line, f)) {
		char *p = line;

		while (*p == ' ' || *p == '\t') p++;
		if (!strncasecmp(p, "INDEX ", 6)) {
			int idx = atoi(p + 6), m = 0, sec = 0, fr = 0, at;

			for (p += 6; *p == ' '; p++) ;
			while (*p && *p != ' ' && *p != '\n') p++;
			while (*p == ' ') p++;
			sscanf(p, "%d:%d:%d", &m, &sec, &fr);
			at = (m * 60 + sec) * 75 + fr;
			if (cur.first == -1) {
				cur.first = at;
				if (!strcmp(cur.file, prev.file)) {
					prev.count = cur.first - prev.first;
					cur.file_off += (long)prev.count * prev.ss;
				}
			}
			if (idx == 1) {
				cur.pregap = at - cur.first;
				if (cur.id == want || (want == DC_FIRST_DATA && cur.data)) {
					want = cur.id;
					done = true;
				}
			}
		} else if (!strncasecmp(p, "TRACK ", 6)) {
			if (cur.ss) prev = cur;
			p += 6;
			cur.id = atoi(p);
			cur.pregap = -1;
			cur.first = -1;
			while (*p && *p != ' ') p++;
			while (*p == ' ') p++;
			memset(cur.mode, 0, sizeof cur.mode);
			snprintf(cur.mode, sizeof cur.mode, "%.*s", (int)strcspn(p, " \t\r\n"), p);
			cur.data = !strncmp(cur.mode, "MODE", 4);
			cur.ss = cur.data ? atol(cur.mode + 6) : 2352;
		} else if (!strncasecmp(p, "FILE ", 5)) {
			char *q;

			if (cur.ss) {
				prev = cur;
				if (prev.count == 0 && prev.ss > 0) {
					char pb[1024];

					sibling(path, prev.file, pb, sizeof pb);
					prev.count = (int)(file_bytes(pb) / prev.ss) - prev.first;
				}
			}
			memset(&cur, 0, sizeof cur);
			cur.file_first = prev.file_first + prev.first + prev.count;
			for (p += 5; *p == ' '; p++) ;
			if (*p == '"') {
				q = strchr(++p, '"');
				if (!q) q = p + strcspn(p, "\r\n");
			} else {
				q = p + strcspn(p, " \t\r\n");
			}
			snprintf(cur.file, sizeof cur.file, "%.*s", (int)(q - p), p);
		}
	}
	fclose(f);
	if (want == DC_LAST && !done) want = cur.id;
	if (!cur.id || cur.id != want || !cur.file[0]) return false;

	sibling(path, cur.file, bin, sizeof bin);
	if (!(t->f = disc_open_file(bin))) return false;
	pregap = cur.pregap > 0 ? cur.pregap : 0;
	first = cur.file_first + cur.first;
	off = cur.file_off;
	/* cdreader_determine_sector_size, at sector 16 past the pregap */
	if (!dc_peek(t->f, off, 16 + pregap, 2352, h)) {
		t->ss = 0;
	} else if (!memcmp(h, cd_sync, 12)) {
		t->ss = 2352;
		t->hdr = memcmp(h + 25, "CD001", 5) ? 16 : 24;
		first = dc_msf(h) - 16 - pregap;
	} else if (dc_peek(t->f, off, 16 + pregap, 2336, h) && !memcmp(h, cd_sync, 12)) {
		t->ss = 2336;
		t->hdr = memcmp(h + 25, "CD001", 5) ? 16 : 24;
		first = dc_msf(h) - 16 - pregap;
	} else if (dc_peek(t->f, off, 16 + pregap, 2048, h) && !memcmp(h + 1, "CD001", 5)) {
		t->ss = 2048;
		t->hdr = 0;
	} else {
		t->ss = 0;
	}
	if (!t->ss) {
		/* the sheet's own word for it, cdreader_open_bin */
		if (!strncmp(cur.mode, "MODE2/2352", 10))      { t->ss = 2352; t->hdr = 24; }
		else if (!strncmp(cur.mode, "MODE1/2048", 10)) { t->ss = 2048; t->hdr = 0; }
		else if (!strncmp(cur.mode, "MODE2/2336", 10)) { t->ss = 2336; t->hdr = 8; }
		else if (!strncmp(cur.mode, "MODE1/2352", 10)) { t->ss = 2352; t->hdr = 16; }
		else if (!strncmp(cur.mode, "AUDIO", 5))       { t->ss = 2352; t->hdr = 0; }
		else { disc_close(t->f); t->f = NULL; return false; }
	}
	/* From the end of the pregap, so the walk's sector 16 is the track's */
	t->first = first + pregap;
	t->offset = off + pregap * t->ss;
	return true;
}

/* Track `want` of a .chd, where RetroArch's CHD reader puts it
 * (chd_cd_track). Its header is libretro-common's cdfs_determine_sector_size:
 * "CD001" 25 bytes into sector 16 is Mode 2, anything else on a raw track
 * Mode 1. */
static bool dc_chd_track(const char *path, int want, psx_track *t)
{
	unsigned char h[32];
	chd_track ti;

	if (!(t->f = disc_open_chd_track(path, want, &ti))) return false;
	t->ss = (long)ti.datasize;
	t->first = (long)ti.lba;
	t->count = (long)ti.frames;
	if (ti.datasize == 2352)
		t->hdr = disc_read(t->f, 16 * 2352, h, sizeof h) == sizeof h &&
		         !memcmp(h + 25, "CD001", 5) ? 24 : 16;
	else
		t->hdr = ti.datasize == 2336 ? 8 : 0;
	return true;
}

/* A DiscJuggler .cdi, as Flycast reads one (imgread/cdi.cpp and its
 * deps/chdpsr): the file's last 8 bytes are the version and where the
 * descriptor starts (counted back from the end for version 3.5), which lists
 * each session's tracks - mode (0 audio, 1 Mode 1, 2 Mode 2), sector size,
 * pregap, length, first sector and length in the file - and the file holds
 * the tracks one after another, pregaps and all. Flycast's track starts at
 * its sector plus its pregap, counted from -150, as a disc's are. */
#define CDI_V2  0x80000004u
#define CDI_V3  0x80000005u
#define CDI_V35 0x80000006u
#define CDI_DESC_MAX (1u << 20)

typedef struct {
	const unsigned char *p;
	size_t n, at;
	bool bad;
} cdi_cur;

static void cdi_skip(cdi_cur *c, size_t k)
{
	if (c->bad || k > c->n - c->at) { c->bad = true; c->at = c->n; return; }
	c->at += k;
}

static uint32_t cdi_get(cdi_cur *c, size_t k)
{
	uint32_t v = 0;
	size_t i;

	if (c->bad || k > c->n - c->at) { c->bad = true; c->at = c->n; return 0; }
	for (i = 0; i < k; i++) v |= (uint32_t)c->p[c->at + i] << (8 * i);
	c->at += k;
	return v;
}

typedef struct {
	uint32_t mode, ss, pregap, length, lba, total;
	uint64_t pos;
} cdi_trk;

/* One track's entry, cdipsr's CDI_read_track */
static bool cdi_entry(cdi_cur *c, uint32_t version, cdi_trk *tr)
{
	static const unsigned char mark[10] = { 0, 0, 1, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF };
	uint32_t size_code;

	if (cdi_get(c, 4)) cdi_skip(c, 8);
	if (c->bad || c->n - c->at < 20 || memcmp(c->p + c->at, mark, 10) ||
	    memcmp(c->p + c->at + 10, mark, 10))
		return false;
	cdi_skip(c, 20 + 4);
	cdi_skip(c, cdi_get(c, 1));               /* the file name */
	cdi_skip(c, 11 + 4 + 4);
	if (cdi_get(c, 4) == 0x80000000u) cdi_skip(c, 8);
	cdi_skip(c, 2);
	tr->pregap = cdi_get(c, 4);
	tr->length = cdi_get(c, 4);
	cdi_skip(c, 6);
	tr->mode = cdi_get(c, 4);
	cdi_skip(c, 12);
	tr->lba = cdi_get(c, 4);
	tr->total = cdi_get(c, 4);
	cdi_skip(c, 16);
	size_code = cdi_get(c, 4);
	tr->ss = size_code == 0 ? 2048 : size_code == 1 ? 2336 : size_code == 2 ? 2352 :
	         size_code == 4 ? 2448 : 0;
	cdi_skip(c, 29);
	if (version != CDI_V2) {
		cdi_skip(c, 5);
		if (cdi_get(c, 4) == 0xFFFFFFFFu) cdi_skip(c, 78);
	}
	return !c->bad;
}

static bool dc_cdi_track(const char *path, int want, psx_track *t)
{
	disc *f = disc_open_file(path);
	unsigned char tail[8], *desc = NULL, h[32];
	uint64_t size, start, pos = 0;
	uint32_t version, hoff, sessions, s, k, n = 0;
	cdi_trk tr, pick;
	cdi_cur c;
	bool found = false;

	if (!f) return false;
	size = disc_size(f);
	if (size < 8 || disc_read(f, size - 8, tail, 8) != 8) goto fail;
	version = rd32(tail);
	hoff = rd32(tail + 4);
	if ((version != CDI_V2 && version != CDI_V3 && version != CDI_V35) || !hoff) goto fail;
	start = version == CDI_V35 ? size - hoff : hoff;
	if (hoff > size || start >= size || size - start > CDI_DESC_MAX) goto fail;
	if (!(desc = malloc((size_t)(size - start)))) goto fail;
	if (disc_read(f, start, desc, (size_t)(size - start)) != size - start) goto fail;
	c.p = desc;
	c.n = (size_t)(size - start);
	c.at = 0;
	c.bad = false;

	sessions = cdi_get(&c, 2);
	for (s = 0; s < sessions && !c.bad; s++) {
		uint32_t tracks = cdi_get(&c, 2);

		for (k = 0; k < tracks; k++) {
			if (!cdi_entry(&c, version, &tr) || !tr.ss) goto fail;
			tr.pos = pos;
			n++;
			if (!found && ((want > 0 && (uint32_t)want == n) ||
			               (want == DC_FIRST_DATA && tr.mode != 0))) {
				pick = tr;
				found = true;
			}
			if (want == DC_LAST) { pick = tr; found = true; }
			/* where the next track is; a cut-short one's total is bytes
			 * there, as Flycast takes it */
			if ((uint64_t)tr.total < (uint64_t)tr.length + tr.pregap)
				pos += tr.total;
			else
				pos += (uint64_t)tr.total * tr.ss;
		}
		cdi_skip(&c, 4 + 8 + (version != CDI_V2 ? 1 : 0));
	}
	free(desc);
	desc = NULL;
	if (!found || c.bad) goto fail;

	t->f = f;
	t->ss = (long)pick.ss;
	t->offset = (long)(pick.pos + (uint64_t)pick.pregap * pick.ss);
	t->first = (long)pick.lba + (long)pick.pregap - 150;
	t->count = (long)pick.length;
	/* Flycast's ConvertSector: a raw sector's mode byte says where its 2048
	 * are; 2336 bytes are Mode 2's, past their 8-byte subheader */
	if (pick.ss == 2352 || pick.ss == 2448)
		t->hdr = disc_read(f, (uint64_t)t->offset + 16u * pick.ss, h, 16) == 16 &&
		         h[15] == 1 ? 16 : 24;
	else
		t->hdr = pick.ss == 2336 ? 8 : 0;
	return true;

fail:
	free(desc);
	disc_close(f);
	return false;
}

static bool dc_open(const char *path, int want, psx_track *t)
{
	memset(t, 0, sizeof *t);
	if (has_ext(path, "gdi")) return dc_gdi_track(path, want, t);
	if (has_ext(path, "cue")) return dc_cue_track(path, want, t);
	if (has_ext(path, "chd")) return dc_chd_track(path, want, t);
	if (has_ext(path, "cdi")) return dc_cdi_track(path, want, t);
	return false;
}

static bool ra_hash_dc(const char *path, char *out)
{
	static const char katana[] = "SEGA SEGAKATANA ";
	unsigned char ip[256], probe[1];
	char exe[17];
	psx_track t;
	uint32_t sector, size = 0;
	md5_ctx m;
	int n;
	bool ok;

	memset(ip, 0, sizeof ip);
	if (dc_open(path, 3, &t)) psx_read(&t, t.first, ip, sizeof ip);
	if (memcmp(ip, katana, 16)) {
		disc_close(t.f);
		if (!dc_open(path, DC_FIRST_DATA, &t)) return false;
		psx_read(&t, t.first, ip, sizeof ip);
		if (memcmp(ip, katana, 16)) { disc_close(t.f); return false; }
	}
	md5_init(&m);
	md5_update(&m, ip, sizeof ip);

	/* The boot file's name, up to a space, 16 bytes at most */
	for (n = 0; n < 16 && !isspace(ip[96 + n]); n++) ;
	if (!n) { disc_close(t.f); return false; }
	memcpy(exe, ip + 96, (size_t)n);
	exe[n] = '\0';
	sector = psx_find(&t, exe, &size);
	if (!sector) { disc_close(t.f); return false; }
	if (!psx_read(&t, (long)sector, probe, sizeof probe)) {
		disc_close(t.f);
		if (!dc_open(path, DC_LAST, &t)) return false;
	}
	ok = cd_add_file(&t, &m, sector, size);
	disc_close(t.f);
	if (ok) md5_hex(&m, out);
	return ok;
}

void ra_md5_hex(const void *data, size_t len, char *out)
{
	md5_ctx m;

	md5_init(&m);
	md5_update(&m, data, len);
	md5_hex(&m, out);
}

/* What ra_hash_rom refuses by the file's name, before it opens it: a disc it
 * has no walk for. The PlayStation's walk reads a .cue (and an .m3u's first
 * disc), a bare .bin, .iso or .img, a .chd and a .pbp (ra_hash_psx); the
 * PSP's an .iso, .cso, .chd or .pbp (ra_hash_psp); the Dreamcast's a .gdi,
 * .cue, .chd or .cdi (ra_hash_dc); an arcade set is hashed by its name,
 * whatever the file is.
 *
 * Any other system's disc is refused UNREAD. Only those three have a walk
 * here, and the cartridge path reads a file whole: a Sega CD or Neo Geo CD
 * .chd is 100 to 430 MB, which on the Pixel 2's 1 GB, beside the game that
 * is running, is the whole problem, for a hash of the container that no game
 * has. .bin is not on the list, because it is also what a Genesis or 2600
 * cartridge is called. */
static bool refused_by_name(const char *tag, const char *path)
{
	if (!strcmp(tag, "PS1"))
		return !has_ext(path, "m3u") && !has_ext(path, "cue") &&
		       !has_ext(path, "bin") && !has_ext(path, "iso") &&
		       !has_ext(path, "img") && !has_ext(path, "chd") &&
		       !has_ext(path, "pbp");
	if (!strcmp(tag, "PSP"))
		return !has_ext(path, "iso") && !has_ext(path, "cso") &&
		       !has_ext(path, "chd") && !has_ext(path, "pbp");
	if (!strcmp(tag, "DC"))
		return !has_ext(path, "gdi") && !has_ext(path, "cue") &&
		       !has_ext(path, "chd") && !has_ext(path, "cdi");
	if (!strcmp(tag, "ARCADE") || !strcmp(tag, "NEOGEO") || !strcmp(tag, "MAME"))
		return false;
	return is_disc_image(path);
}

ra_hash_kind ra_hash_support(const char *tag, const char *path, const char *exts)
{
	const char *p = exts;

	if (!tag || !path) return RA_HASH_NO_FILE;
	if (!refused_by_name(tag, path)) return RA_HASH_OK;

	/* Refused. Is any kind of file this shelf takes not? Asked of a name made
	 * of each of its extensions, by the same test, in systems.cfg's own
	 * spelling of a list (commas, or spaces, as src/library.c reads it). */
	while (p && *p) {
		char probe[40];
		size_t n = 0;

		while (*p == ',' || *p == ' ') p++;
		while (p[n] && p[n] != ',' && p[n] != ' ') n++;
		if (n && n < sizeof probe - 3) {
			snprintf(probe, sizeof probe, "x.%.*s", (int)n, p);
			if (!refused_by_name(tag, probe)) return RA_HASH_NO_FILE;
		}
		p += n;
	}
	/* An empty list takes every file, so another kind may be read */
	return exts && *exts ? RA_HASH_NO_SYSTEM : RA_HASH_NO_FILE;
}

/* A DS game, rcheevos's rc_hash_nintendo_ds: not the whole file, but what
 * the cartridge runs - the header's first 0x160 bytes, the ARM9 code, the
 * ARM7 code and the 0xA00 bytes of icon and titles, each where the header
 * puts it. Hashed whole, a DS game had a hash RetroAchievements has never
 * seen, so none ever found its set (found 2026-11-16 by MENU > Achievements'
 * Download: six real DS games, all "none"). A SuperCard's own 512-byte
 * header comes first on some dumps and is passed over. Codes past 16 MB
 * between them are no DS ROM (rcheevos's own test), and a part the file
 * ends inside is filled out with zeros, as rcheevos fills a short icon. */
#define NDS_HEAD     0x160
#define NDS_ICON     0xA00
#define NDS_CODE_MAX (16u * 1024 * 1024)

static uint32_t nds_u32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* `size` bytes of the image from `at`, zeros past its end */
static void nds_part(md5_ctx *m, const unsigned char *d, size_t len, uint64_t at, uint32_t size)
{
	static const unsigned char zero[4096];

	while (size) {
		uint32_t n;

		if (at < len) {
			n = (uint32_t)((uint64_t)len - at < size ? (uint64_t)len - at : size);
			md5_update(m, d + at, n);
		} else {
			n = size < sizeof zero ? size : (uint32_t)sizeof zero;
			md5_update(m, zero, n);
		}
		at += n;
		size -= n;
	}
}

static bool ra_hash_nds(const unsigned char *d, size_t len, char *out)
{
	const unsigned char *h = d;
	uint64_t off = 0;
	uint32_t arm9_addr, arm9_size, arm7_addr, arm7_size, icon_addr;
	unsigned char sc[512];
	md5_ctx m;

	if (len < 512) return false;
	if (d[0] == 0x2E && d[1] == 0x00 && d[2] == 0x00 && d[3] == 0xEA &&
	    d[0xB0] == 0x44 && d[0xB1] == 0x46 && d[0xB2] == 0x96 && d[0xB3] == 0x00) {
		/* the SuperCard's header: the cartridge's own is the next 512 */
		off = 512;
		memset(sc, 0, sizeof sc);
		memcpy(sc, d + 512, len - 512 < sizeof sc ? len - 512 : sizeof sc);
		h = sc;
	}
	arm9_addr = nds_u32(h + 0x20);
	arm9_size = nds_u32(h + 0x2C);
	arm7_addr = nds_u32(h + 0x30);
	arm7_size = nds_u32(h + 0x3C);
	icon_addr = nds_u32(h + 0x68);
	if ((uint64_t)arm9_size + arm7_size > NDS_CODE_MAX) return false;

	md5_init(&m);
	md5_update(&m, h, NDS_HEAD);
	nds_part(&m, d, len, off + arm9_addr, arm9_size);
	nds_part(&m, d, len, off + arm7_addr, arm7_size);
	nds_part(&m, d, len, off + icon_addr, NDS_ICON);
	md5_hex(&m, out);
	return true;
}

bool ra_hash_rom(const char *path, const char *tag, char *out)
{
	unsigned char *data = NULL;
	size_t len = 0, skip;
	md5_ctx m;

	if (!path || !tag || !out) return false;
	out[0] = '\0';

	/* The one test ra_hash_support asks too, so the rows and this cannot
	 * disagree about which games have no hash */
	if (refused_by_name(tag, path)) return false;

	/* Before anything reads the file: a disc is hashed by what is on it. */
	if (!strcmp(tag, "PS1")) return ra_hash_psx(path, out);
	if (!strcmp(tag, "PSP")) return ra_hash_psp(path, out);
	if (!strcmp(tag, "DC"))  return ra_hash_dc(path, out);

	/* An arcade set is hashed by its NAME, rcheevos's rc_hash_arcade: md5 of
	 * "mslug" for mslug.zip. The chips inside are FinalBurn Neo's to check,
	 * and reading the largest of them, up to 64 MB for a late Neo Geo set,
	 * would hash nothing RetroAchievements has. */
	if (!strcmp(tag, "ARCADE") || !strcmp(tag, "NEOGEO") || !strcmp(tag, "MAME")) {
		const char *base = strrchr(path, '/');
		const char *dot;

		base = base ? base + 1 : path;
		dot = strrchr(base, '.');
		md5_init(&m);
		md5_update(&m, base, dot ? (size_t)(dot - base) : strlen(base));
		md5_hex(&m, out);
		return true;
	}

	/* Any other disc was refused above, unread (refused_by_name) */

	if (is_zip(path)) {
		data = zip_largest(path, &len);
		if (!data) return false;
	} else {
		FILE *f = fopen(path, "rb");
		long sz;

		if (!f) return false;
		fseek(f, 0, SEEK_END);
		sz = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (sz <= 0) { fclose(f); return false; }
		data = malloc((size_t)sz);
		if (!data) { fclose(f); return false; }
		if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
			free(data); fclose(f); return false;
		}
		fclose(f);
		len = (size_t)sz;
	}

	if (!strcmp(tag, "NDS")) {
		bool ok = ra_hash_nds(data, len, out);

		free(data);
		return ok;
	}
	if (!strcmp(tag, "N64") && !n64_to_z64(data, len)) { free(data); return false; }
	skip = ra_skip(tag, data, len);
	if (skip >= len) { free(data); return false; }

	md5_init(&m);
	md5_update(&m, data + skip, len - skip);
	md5_hex(&m, out);
	free(data);
	return true;
}
