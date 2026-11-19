/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8: Splore's entry, the page that says how to add PICO-8, and the
 * zip a player copied by hand. See src/pico8.h. */
#include <ctype.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "pico8.h"

#include "i18n.h"

/* ---------- Splore's entry ------------------------------------------------ */

bool splore_file(const char *file)
{
	return file && !strcasecmp(file, SPLORE_FILE);
}

bool splore_listing_has(const game_list *l)
{
	int i;

	for (i = 0; i < l->count; i++)
		if (splore_file(l->items[i].file)) return true;
	return false;
}

bool splore_listing_add(game_list *l)
{
	game_entry *more;

	if (splore_listing_has(l)) return true;
	more = realloc(l->items, (size_t)(l->count + 1) * sizeof *more);
	if (!more) return false;
	l->items = more;
	memmove(&more[1], &more[0], (size_t)l->count * sizeof *more);
	memset(&more[0], 0, sizeof *more);
	/* As lib_scan_listing makes an entry from a file: the name its stem, the
	 * title left for lib_scan_finish */
	snprintf(more[0].file, sizeof more[0].file, "%s", SPLORE_FILE);
	snprintf(more[0].name, sizeof more[0].name, "%s", SPLORE_NAME);
	more[0].added = LIB_ADDED_UNKNOWN;
	l->count++;
	l->scanned = true;
	return true;
}

void splore_pin(game_entry *items, int n)
{
	game_entry e;
	int i;

	for (i = 0; i < n && !splore_file(items[i].file); i++) {}
	if (i <= 0 || i >= n) return;
	e = items[i];
	memmove(&items[1], &items[0], (size_t)i * sizeof *items);
	items[0] = e;
}

/* ---------- the page ------------------------------------------------------ */

static void note(char notes[P8_NOTES][P8_NOTE_LEN], int *n, const char *text)
{
	if (*n < P8_NOTES) snprintf(notes[(*n)++], P8_NOTE_LEN, "%s", text);
}

void p8_page_build(su_page *p, char notes[P8_NOTES][P8_NOTE_LEN], int *nnotes)
{
	memset(p, 0, sizeof *p);
	*nnotes = 0;
	/* "PICO-8" where the versions go: its name, which no language changes */
	snprintf(p->from, sizeof p->from, "%s", "PICO-8");
	snprintf(p->sub, sizeof p->sub, "%s", T(S_PICO8_PAGE_SUB));
	p->notes = true;
	note(notes, nnotes, T(S_PICO8_POINT_WHAT));
	note(notes, nnotes, T(S_PICO8_POINT_WHOSE));
	note(notes, nnotes, T(S_PICO8_POINT_HOW));
	if (p->nkeys < SU_MAX_KEYS) {
		snprintf(p->keys[0].button, sizeof p->keys[0].button, "%s", "B");
		snprintf(p->keys[0].label, sizeof p->keys[0].label, "%s", T(S_UPDATE_BACK));
		p->keys[0].act = SUA_BACK;
		p->nkeys = 1;
	}
}

/* ---------- the zip's rules ----------------------------------------------- */

static bool version_char(char c)
{
	return isdigit((unsigned char)c) || c == '.' || (c >= 'a' && c <= 'z');
}

/* A version token at `p`: a digit first, then digits, dots and lowercase
 * letters ("0.2.7", "0.2.6b"), into `out`; false for none */
static bool version_at(const char *p, size_t len, char *out, size_t n)
{
	size_t k = 0;

	if (!len || !isdigit((unsigned char)p[0])) return false;
	while (k < len && version_char(p[k])) k++;
	while (k && p[k - 1] == '.') k--;
	if (!k || k >= n) return false;
	memcpy(out, p, k);
	out[k] = 0;
	return true;
}

bool p8_version_from_name(const char *name, char *out, size_t n)
{
	const char *b = strrchr(name, '/'), *p;

	b = b ? b + 1 : name;
	for (p = b; *p; p++)
		if (!strncasecmp(p, "pico-8_", 7)) {
			const char *v = p + 7;
			size_t len = strcspn(v, "_");

			return version_at(v, len, out, n);
		}
	return false;
}

bool p8_version_from_manual(const char *text, size_t len, char *out, size_t n)
{
	size_t i;

	/* "PICO-8 v0.2.7" in its first lines: the first "PICO-8" followed by
	 * spaces, an optional v, and a version */
	for (i = 0; i + 6 < len && i < 4096; i++) {
		size_t k;

		if (strncmp(text + i, "PICO-8", 6)) continue;
		k = i + 6;
		while (k < len && text[k] == ' ') k++;
		if (k < len && (text[k] == 'v' || text[k] == 'V')) k++;
		if (k > i + 6 && version_at(text + k, len - k, out, n)) return true;
	}
	return false;
}

bool p8_elf_aarch64(const unsigned char *h, size_t n)
{
	/* ELF, 64-bit (EI_CLASS 2), little-endian (EI_DATA 1), e_machine 183 */
	return n >= 20 && h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F' &&
	       h[4] == 2 && h[5] == 1 && (h[18] | h[19] << 8) == 183;
}

const char *p8_zip_member(const char *path)
{
	const char *b = strchr(path, '/');

	/* At the top, or one folder down ("pico-8/"), never deeper: a Mac's
	 * __MACOSX/pico-8/._pico8_64 is not the program */
	if (b) {
		if (b == path || strchr(b + 1, '/')) return NULL;
		b++;
	} else {
		b = path;
	}
	if (!strcmp(b, P8_PROGRAM)) return P8_PROGRAM;
	if (!strcmp(b, P8_DATA)) return P8_DATA;
	if (!strcmp(b, P8_LICENSE)) return P8_LICENSE;
	if (!strcmp(b, "pico-8_manual.txt") || !strcmp(b, "pico-8.txt")) return "";
	return NULL;
}

/* ---------- a zip copied by hand ------------------------------------------ */

static uint32_t le16(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const unsigned char *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

typedef struct {
	const char *as;            /* its name in Bios/PICO-8; "" the manual */
	uint32_t method, csize, usize, off;
	bool found;
} zmember;

enum { ZM_PROGRAM, ZM_DATA, ZM_LICENSE, ZM_MANUAL, ZM_COUNT };

/* The zip's central directory: each member p8_zip_member takes, where it is.
 * False for a file that is not a zip, or a damaged one. */
static bool zip_members(FILE *f, zmember m[ZM_COUNT])
{
	unsigned char *tail = NULL, *cd = NULL;
	long size, n, i, at = -1;
	uint32_t cd_size, cd_off, count, k;
	bool ok = false;

	if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < 22) return false;
	n = size < 65557 ? size : 65557;
	if (!(tail = malloc((size_t)n)) || fseek(f, size - n, SEEK_SET) != 0 ||
	    fread(tail, 1, (size_t)n, f) != (size_t)n)
		goto out;
	for (i = n - 22; i >= 0 && at < 0; i--)
		if (le32(tail + i) == 0x06054b50) at = i;
	if (at < 0) goto out;
	count = le16(tail + at + 10);
	cd_size = le32(tail + at + 12);
	cd_off = le32(tail + at + 16);
	if (cd_size > (4u << 20) || (long)cd_off + (long)cd_size > size) goto out;
	if (!(cd = malloc(cd_size ? cd_size : 1)) || fseek(f, (long)cd_off, SEEK_SET) != 0 ||
	    fread(cd, 1, cd_size, f) != cd_size)
		goto out;
	for (k = 0, i = 0; k < count; k++) {
		uint32_t nlen, xlen, clen;
		char name[512];
		const char *as;
		int slot;

		if (i + 46 > (long)cd_size || le32(cd + i) != 0x02014b50) goto out;
		nlen = le16(cd + i + 28);
		xlen = le16(cd + i + 30);
		clen = le16(cd + i + 32);
		if (i + 46 + (long)nlen > (long)cd_size) goto out;
		if (nlen < sizeof name) {
			memcpy(name, cd + i + 46, nlen);
			name[nlen] = 0;
			if ((as = p8_zip_member(name))) {
				slot = !strcmp(as, P8_PROGRAM) ? ZM_PROGRAM : !strcmp(as, P8_DATA) ? ZM_DATA :
				       !strcmp(as, P8_LICENSE) ? ZM_LICENSE : ZM_MANUAL;
				if (!m[slot].found) {
					m[slot].as = as;
					m[slot].method = le16(cd + i + 10);
					m[slot].csize = le32(cd + i + 20);
					m[slot].usize = le32(cd + i + 24);
					m[slot].off = le32(cd + i + 42);
					m[slot].found = true;
				}
			}
		}
		i += 46 + (long)nlen + (long)xlen + (long)clen;
	}
	ok = true;
out:
	free(tail);
	free(cd);
	return ok;
}

/* A member's bytes, whole (stored or deflated), or NULL */
static unsigned char *zip_read(FILE *f, const zmember *m)
{
	unsigned char lh[30], *in = NULL, *out = NULL;
	z_stream z;

	if (m->usize > P8_MAX_BYTES || m->csize > P8_MAX_BYTES) return NULL;
	if (fseek(f, (long)m->off, SEEK_SET) != 0 || fread(lh, 1, 30, f) != 30 ||
	    le32(lh) != 0x04034b50 ||
	    fseek(f, (long)(le16(lh + 26) + le16(lh + 28)), SEEK_CUR) != 0)
		return NULL;
	if (!(out = malloc(m->usize ? m->usize : 1))) return NULL;
	if (m->method == 0) {
		if (m->csize != m->usize || fread(out, 1, m->usize, f) != m->usize) goto fail;
		return out;
	}
	if (m->method != 8 || !(in = malloc(m->csize ? m->csize : 1)) ||
	    fread(in, 1, m->csize, f) != m->csize)
		goto fail;
	memset(&z, 0, sizeof z);
	z.next_in = in;
	z.avail_in = m->csize;
	z.next_out = out;
	z.avail_out = m->usize;
	/* -15: raw deflate, no zlib wrapper, which is what a zip stores */
	if (inflateInit2(&z, -15) != Z_OK) goto fail;
	if (inflate(&z, Z_FINISH) != Z_STREAM_END || z.total_out != m->usize) {
		inflateEnd(&z);
		goto fail;
	}
	inflateEnd(&z);
	free(in);
	return out;
fail:
	free(in);
	free(out);
	return NULL;
}

/* `n` bytes into `dir`/`name`, written whole under a temporary name, synced,
 * then renamed over what was there */
static bool put_file(const char *dir, const char *name, const void *data, size_t n, bool program)
{
	char tmp[1024], path[1024];
	FILE *f;
	bool ok;

	snprintf(tmp, sizeof tmp, "%s/.%s.part", dir, name);
	snprintf(path, sizeof path, "%s/%s", dir, name);
	if (!(f = fopen(tmp, "wb"))) return false;
	ok = fwrite(data, 1, n, f) == n && fflush(f) == 0 && fsync(fileno(f)) == 0;
	if (fclose(f) != 0) ok = false;
	if (ok && program) chmod(tmp, 0755);
	if (!ok || rename(tmp, path) != 0) {
		unlink(tmp);
		return false;
	}
	return true;
}

p8_take p8_take_zip(const char *card, char *version, size_t vn, char *zip, size_t zn)
{
	char dir[768], path[1024], best[256] = "";
	time_t best_t = 0;
	zmember m[ZM_COUNT];
	unsigned char *prog = NULL, *data = NULL, *lic = NULL, *man = NULL;
	struct stat st;
	struct dirent *e;
	p8_take r = P8_TAKE_FAILED;
	FILE *f;
	DIR *d;

	if (version && vn) version[0] = 0;
	if (zip && zn) zip[0] = 0;
	snprintf(dir, sizeof dir, "%s/%s", card, P8_DIR);
	/* Most cards have no Bios/PICO-8: one failed open, and nothing else */
	if (!(d = opendir(dir))) return P8_TAKE_NONE;
	while ((e = readdir(d))) {
		size_t len = strlen(e->d_name);

		if (e->d_name[0] == '.' || len < 5 || strcasecmp(e->d_name + len - 4, ".zip")) continue;
		snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
		if (!best[0] || st.st_mtime > best_t) {
			snprintf(best, sizeof best, "%s", e->d_name);
			best_t = st.st_mtime;
		}
	}
	closedir(d);
	if (!best[0]) return P8_TAKE_NONE;
	/* Taken already: the program there is as new as the zip, or newer */
	snprintf(path, sizeof path, "%s/%s", dir, P8_PROGRAM);
	if (stat(path, &st) == 0 && st.st_mtime >= best_t) return P8_TAKE_NONE;
	if (zip) snprintf(zip, zn, "%s", best);

	snprintf(path, sizeof path, "%s/%s", dir, best);
	if (!(f = fopen(path, "rb"))) return P8_TAKE_FAILED;
	memset(m, 0, sizeof m);
	if (!zip_members(f, m)) { fclose(f); return P8_TAKE_NOT_RASPI; }
	if (!m[ZM_PROGRAM].found || !m[ZM_DATA].found) { fclose(f); return P8_TAKE_NOT_RASPI; }
	if (!(prog = zip_read(f, &m[ZM_PROGRAM])) || !(data = zip_read(f, &m[ZM_DATA]))) goto done;
	if (!p8_elf_aarch64(prog, m[ZM_PROGRAM].usize)) { r = P8_TAKE_NOT_RASPI; goto done; }
	if (m[ZM_LICENSE].found) lic = zip_read(f, &m[ZM_LICENSE]);
	if (m[ZM_MANUAL].found) man = zip_read(f, &m[ZM_MANUAL]);
	if (version && vn) {
		if (!(man && p8_version_from_manual((const char *)man, m[ZM_MANUAL].usize, version, vn)))
			p8_version_from_name(best, version, vn);
	}
	if (!put_file(dir, P8_DATA, data, m[ZM_DATA].usize, false) ||
	    !put_file(dir, P8_PROGRAM, prog, m[ZM_PROGRAM].usize, true))
		goto done;
	if (lic) put_file(dir, P8_LICENSE, lic, m[ZM_LICENSE].usize, false);
	if (version && version[0]) {
		char line[48];
		int k = snprintf(line, sizeof line, "%s\n", version);

		put_file(dir, P8_VERSION, line, (size_t)k, false);
	} else {
		snprintf(path, sizeof path, "%s/%s", dir, P8_VERSION);
		unlink(path);
	}
	r = P8_TAKE_DONE;
done:
	fclose(f);
	free(prog);
	free(data);
	free(lic);
	free(man);
	return r;
}

void p8_take_message(p8_take t, const char *version, const char *zip, char *out, size_t n)
{
	out[0] = 0;
	switch (t) {
	case P8_TAKE_DONE:
		if (version && version[0]) snprintf(out, n, T(S_PICO8_READY), version);
		else snprintf(out, n, "%s", T(S_PICO8_READY_PLAIN));
		break;
	case P8_TAKE_NOT_RASPI:
		snprintf(out, n, T(S_PICO8_NOT_RASPI), zip ? zip : "");
		break;
	case P8_TAKE_FAILED:
		snprintf(out, n, T(S_PICO8_TAKE_FAILED), zip ? zip : "");
		break;
	default:
		break;
	}
}
