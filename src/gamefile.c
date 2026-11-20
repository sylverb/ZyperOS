/* SPDX-License-Identifier: GPL-3.0-only */
#include "gamefile.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "biosfile.h"
#include "i18n.h"

/* The kinds with a least size, and why (gamefile.h) */
static const struct { const char *ext; unsigned least; } LEAST[] = {
	{ "zip", 22 },    /* the end of central directory record */
	{ "7z", 32 },     /* the signature header */
	{ "chd", 76 },    /* the shortest CHD header, version 1 */
	{ "cso", 24 },    /* the CISO header */
	{ "pbp", 40 },    /* the PBP header and its eight offsets */
	{ "iso", 2048 },  /* one sector of a disc */
};

unsigned gamefile_least(const char *name)
{
	const char *dot = name ? strrchr(name, '.') : NULL;
	size_t i;

	if (!dot || strchr(dot, '/')) return 1;
	for (i = 0; i < sizeof LEAST / sizeof LEAST[0]; i++)
		if (!strcasecmp(dot + 1, LEAST[i].ext)) return LEAST[i].least;
	return 1;
}

gamefile_state gamefile_check(const char *path, unsigned long long *have)
{
	struct stat st;

	*have = 0;
	if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return GAMEFILE_OK;
	*have = (unsigned long long)st.st_size;
	if (*have == 0) return GAMEFILE_EMPTY;
	if (*have < gamefile_least(path)) return GAMEFILE_SHORT;
	return GAMEFILE_OK;
}

void gamefile_message(gamefile_state st, unsigned long long have, char *out, size_t n)
{
	char size[48];

	out[0] = '\0';
	if (st == GAMEFILE_EMPTY) {
		snprintf(out, n, "%s", T(S_LAUNCH_GAME_EMPTY));
	} else if (st == GAMEFILE_SHORT) {
		bios_size_text(have, size, sizeof size);
		snprintf(out, n, T(S_LAUNCH_GAME_SHORT), size);
	}
}

static unsigned short rd16(const unsigned char *p) { return (unsigned short)(p[0] | p[1] << 8); }
static unsigned rd32(const unsigned char *p)
{
	return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

int gamefile_zip_method(const char *path)
{
	unsigned char tail[66000 + 22], hdr[46];
	unsigned cd_off = 0, cd_n = 0, best = 0, i;
	int method = -1;
	size_t len, k;
	long size;
	FILE *f;

	if (!path || !(f = fopen(path, "rb"))) return -1;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size < 22) { fclose(f); return -1; }
	len = size < (long)sizeof tail ? (size_t)size : sizeof tail;
	fseek(f, size - (long)len, SEEK_SET);
	if (fread(tail, 1, len, f) != len) { fclose(f); return -1; }
	/* The end record, from the back: its directory's count and place */
	for (k = len - 22 + 1; k-- > 0; )
		if (tail[k] == 'P' && tail[k + 1] == 'K' && tail[k + 2] == 5 && tail[k + 3] == 6) {
			cd_n = rd16(tail + k + 10);
			cd_off = rd32(tail + k + 16);
			break;
		}
	if (!cd_n || fseek(f, (long)cd_off, SEEK_SET) != 0) { fclose(f); return -1; }
	for (i = 0; i < cd_n; i++) {
		unsigned usize, nlen, skip;
		char last = 0;

		if (fread(hdr, 1, 46, f) != 46 || rd32(hdr) != 0x02014b50) break;
		usize = rd32(hdr + 24);
		nlen = rd16(hdr + 28);
		skip = rd16(hdr + 30) + rd16(hdr + 32);
		if (nlen) {
			if (fseek(f, (long)nlen - 1, SEEK_CUR) != 0 || fread(&last, 1, 1, f) != 1) break;
		}
		if (fseek(f, (long)skip, SEEK_CUR) != 0) break;
		/* A folder, or nothing in it: no game; else the largest, whatever
		 * its method, as Diatom chooses */
		if (!nlen || !usize || last == '/') continue;
		if (usize > best) {
			best = usize;
			method = rd16(hdr + 10);
		}
	}
	fclose(f);
	return method;
}
