/* SPDX-License-Identifier: GPL-3.0-only */
/* Cheats, a kind of MENU > Downloads: src/cheatsdl.h */
#include "cheatsdl.h"

#include "artscrape.h"
#include "db.h"
#include "library.h"
#include "net.h"
#include "platform.h"
#include "rmtree.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define TAIL       65536          /* the zip's end, where its index's place is */
#define MAX_MATCH  8              /* a game's files at most */
#define MAX_COLL   64
#define LOCAL_ROOM  1024           /* a local header's name and extra, generously */
#define ASK_S      60             /* a request's time limit; the index is 4 MB */
#define ENTRY_MAX  (4u * 1024 * 1024)   /* a cheat file is a few KB: anything past this is no cheat file */
static const unsigned BACKOFF[] = { 5000, 15000, 30000 };

typedef struct {
	char *name;                   /* the entry's path in the zip, its own copy */
	const char *base;             /* its file name, after the collection */
	char *norm;                   /* its loose name (art_norm), made when needed */
	int coll;
	unsigned crc, csize, usize, off;
	int method, nlen;
} entry;

enum { P_IDLE, P_TAIL, P_TAIL_WAIT, P_CD_WAIT, P_SYSTEM, P_GAME, P_FETCH, P_FETCH_WAIT, P_END };

static struct {
	bool active, one;
	int phase, problem;
	systems_cfg sys;
	char only[CFG_STR];
	/* the index */
	entry *e;
	int ne;
	char coll[MAX_COLL][128];
	int ncoll;
	unsigned cd_off, cd_size, total;
	/* where it is */
	int si;
	char folder[CFG_STR];
	int ca, cb;                   /* the system's collections, -1 none */
	game_list games;
	int gi;
	char rom[LIB_NAME];
	int match[MAX_MATCH], nm, mi;
	bool want[MAX_MATCH];
	bool had;                     /* the game had files before: an update */
	int got;                      /* its cheats, as they land */
	int range_extra;              /* a local header longer than LOCAL_ROOM */
	/* asking */
	int tries, fail_row;
	unsigned wait_until;
	cheats_progress p;
} g;

/* ---- paths ------------------------------------------------------------------------ */

static void root_path(char *out, size_t n) { snprintf(out, n, "%s/Cheats", P_CARD); }
static void index_path(char *out, size_t n) { snprintf(out, n, "%s/Cheats/.libretro-index", P_CARD); }
static void key_path(char *out, size_t n) { snprintf(out, n, "%s/Cheats/.libretro-index.key", P_CARD); }
static void tmp_path(char *out, size_t n) { snprintf(out, n, "%s/Cheats/.libretro-part", P_CARD); }

static void libretro_dir(const char *folder, char *out, size_t n)
{
	snprintf(out, n, "%s/Cheats/%s/.libretro", P_CARD, folder);
}

static void game_dir(const char *folder, const char *rom, char *out, size_t n)
{
	snprintf(out, n, "%s/Cheats/%s/.libretro/%s", P_CARD, folder, rom);
}

static bool mkdirs(const char *path)
{
	char p[1024];
	size_t i;

	snprintf(p, sizeof p, "%s", path);
	for (i = 1; p[i]; i++) {
		if (p[i] != '/') continue;
		p[i] = '\0';
		mkdir(p, 0755);
		p[i] = '/';
	}
	return mkdir(p, 0755) == 0 || access(p, F_OK) == 0;
}

/* ---- the card's side ------------------------------------------------------------------ */

bool cheats_mapped(const char *folder)
{
	return art_lr_collection(folder, 0) != NULL;
}

static int str_cmp(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

int cheats_game_state(const char *folder, const char *rom_name)
{
	char d[1024], none[1100];

	game_dir(folder, rom_name, d, sizeof d);
	if (access(d, F_OK) != 0) return 0;
	snprintf(none, sizeof none, "%s/.none", d);
	return access(none, F_OK) == 0 ? -1 : 1;
}

void cheats_count_sys(const char *folder, const game_list *gl, cheats_count *out)
{
	char dir[1024];
	char **names = NULL;
	int nn = 0, cap = 0, i;
	DIR *dp;
	struct dirent *de;

	memset(out, 0, sizeof *out);
	out->mapped = cheats_mapped(folder);
	if (!out->mapped || !gl) return;
	out->games = gl->count;
	/* the checked games: one read of the system's .libretro folder */
	libretro_dir(folder, dir, sizeof dir);
	if ((dp = opendir(dir))) {
		while ((de = readdir(dp))) {
			if (de->d_name[0] == '.') continue;
			if (nn == cap) {
				char **nv = realloc(names, (size_t)(cap = cap ? cap * 2 : 64) * sizeof *names);

				if (!nv) break;
				names = nv;
			}
			if (!(names[nn] = strdup(de->d_name))) break;
			nn++;
		}
		closedir(dp);
		if (nn) qsort(names, (size_t)nn, sizeof *names, str_cmp);
	}
	for (i = 0; i < gl->count; i++) {
		const char *n = gl->items[i].name;

		if (!nn || !bsearch(&n, names, (size_t)nn, sizeof *names, str_cmp)) { out->todo++; continue; }
		if (cheats_game_state(folder, n) < 0) out->none++;
		else out->have++;
	}
	for (i = 0; i < nn; i++) free(names[i]);
	free(names);
}

/* ---- the index ------------------------------------------------------------------------- */

static unsigned le16(const unsigned char *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }
static unsigned le32(const unsigned char *p)
{
	return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static unsigned char *read_file(const char *path, size_t *n)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b = NULL;
	long len;

	*n = 0;
	if (!f) return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
	    (b = malloc((size_t)len + 1)) && fread(b, 1, (size_t)len, f) == (size_t)len) {
		*n = (size_t)len;
		b[len] = 0;
	} else {
		free(b);
		b = NULL;
	}
	fclose(f);
	return b;
}

static void index_free(void)
{
	int i;

	for (i = 0; i < g.ne; i++) { free(g.e[i].name); free(g.e[i].norm); }
	free(g.e);
	g.e = NULL;
	g.ne = 0;
}

static int coll_of(const char *name, size_t n)
{
	int i;

	for (i = 0; i < g.ncoll; i++)
		if (strlen(g.coll[i]) == n && !memcmp(g.coll[i], name, n)) return i;
	return -1;
}

/* A file name the card can take as it is: one name with no folder in it,
 * not hidden (so never "." or ".."), no NUL - a list that said "../x" or
 * "a/b" would otherwise write outside the game's folder */
static bool base_ok(const char *b, size_t n)
{
	size_t i;

	if (!n || b[0] == '.') return false;
	for (i = 0; i < n; i++)
		if (b[i] == '/' || b[i] == '\\' || b[i] == '\0') return false;
	return true;
}

static int entry_cmp(const void *a, const void *b)
{
	const entry *x = a, *y = b;

	if (x->coll != y->coll) return x->coll - y->coll;
	return strcmp(x->base, y->base);
}

/* The index's records for the card's collections, sorted */
static bool index_parse(const unsigned char *cd, size_t n)
{
	size_t at = 0;

	index_free();
	while (at + 46 <= n && le32(cd + at) == 0x02014b50) {
		unsigned nlen = le16(cd + at + 28), elen = le16(cd + at + 30), clen = le16(cd + at + 32);
		const char *name = (const char *)cd + at + 46, *slash;
		int c;

		if (at + 46 + nlen > n) break;
		slash = memchr(name, '/', nlen);
		if (slash && nlen > 4 && !memcmp(name + nlen - 4, ".cht", 4) &&
		    (c = coll_of(name, (size_t)(slash - name))) >= 0 && base_ok(slash + 1, (size_t)(name + nlen - slash - 1)) &&
		    le32(cd + at + 20) <= ENTRY_MAX && le32(cd + at + 24) <= ENTRY_MAX) {
			entry *grown = g.ne % 256 ? g.e : realloc(g.e, sizeof *g.e * (size_t)(g.ne + 256));
			entry *e;

			if (!grown) return false;
			g.e = grown;
			e = &g.e[g.ne];
			memset(e, 0, sizeof *e);
			if (!(e->name = malloc(nlen + 1))) return false;
			memcpy(e->name, name, nlen);
			e->name[nlen] = '\0';
			e->base = e->name + (slash - name) + 1;
			e->coll = c;
			e->method = (int)le16(cd + at + 10);
			e->crc = le32(cd + at + 16);
			e->csize = le32(cd + at + 20);
			e->usize = le32(cd + at + 24);
			e->off = le32(cd + at + 42);
			e->nlen = (int)nlen;
			g.ne++;
		}
		at += 46 + nlen + elen + clen;
	}
	if (g.ne) qsort(g.e, (size_t)g.ne, sizeof *g.e, entry_cmp);
	fprintf(stderr, "cheats: the index has %d files for this card's %d collections\n", g.ne, g.ncoll);
	return true;
}

/* The zip's end: where its index is and how long. False when it is not a
 * zip's end. */
static bool tail_parse(const unsigned char *b, size_t n)
{
	size_t i;

	if (n < 22) return false;
	for (i = n - 22 + 1; i-- > 0;) {
		if (le32(b + i) != 0x06054b50) continue;
		g.total = le16(b + i + 10);
		g.cd_size = le32(b + i + 12);
		g.cd_off = le32(b + i + 16);
		return g.cd_size > 0;
	}
	return false;
}

/* The index kept on the card, if it is the one the zip has now */
static bool index_cached(void)
{
	char kp[1024], ip[1024], want[64], have[64] = "";
	unsigned char *cd;
	size_t n;
	FILE *f;
	bool ok;

	key_path(kp, sizeof kp);
	index_path(ip, sizeof ip);
	snprintf(want, sizeof want, "%u %u %u", g.cd_off, g.cd_size, g.total);
	if (!(f = fopen(kp, "r"))) return false;
	if (!fgets(have, sizeof have, f)) have[0] = '\0';
	fclose(f);
	have[strcspn(have, "\r\n")] = '\0';
	if (strcmp(have, want)) return false;
	if (!(cd = read_file(ip, &n)) || n != g.cd_size) { free(cd); return false; }
	ok = index_parse(cd, n);
	free(cd);
	return ok;
}

static void index_keep(void)
{
	char kp[1024];
	FILE *f;

	key_path(kp, sizeof kp);
	if ((f = fopen(kp, "w"))) {
		fprintf(f, "%u %u %u\n", g.cd_off, g.cd_size, g.total);
		fclose(f);
	}
}

/* ---- matching -------------------------------------------------------------------------- */

static void entry_noext(const entry *e, char *out, size_t n)
{
	size_t l = strlen(e->base);

	snprintf(out, n, "%.*s", (int)(l > 4 ? l - 4 : l), e->base);
}

static int lower_bound(int coll, const char *base)
{
	int lo = 0, hi = g.ne;

	while (lo < hi) {
		int mid = (lo + hi) / 2;
		const entry *e = &g.e[mid];

		if (e->coll < coll || (e->coll == coll && strcmp(e->base, base) < 0)) lo = mid + 1;
		else hi = mid;
	}
	return lo;
}

/* The ROM's files in one collection: its name exactly, then its name with a
 * code type after it ("Super Mario World (USA) (Game Genie)") */
static void match_exact(int coll, const char *rom)
{
	size_t rl = strlen(rom);
	int i;

	if (coll < 0) return;
	for (i = lower_bound(coll, rom); i < g.ne && g.nm < MAX_MATCH; i++) {
		char nx[512];

		if (g.e[i].coll != coll || strncmp(g.e[i].base, rom, rl)) break;
		entry_noext(&g.e[i], nx, sizeof nx);
		if (!strcmp(nx, rom) || !strncmp(nx + rl, " (", 2)) g.match[g.nm++] = i;
	}
}

/* Else its title in libretro's loose rule, the same region when it has one */
static void match_loose(int coll, const char *rom)
{
	char norm[512], region[64] = "";
	const char *o = strchr(rom, '('), *c = o ? strchr(o, ')') : NULL;
	int i;

	if (coll < 0) return;
	art_norm(rom, norm, sizeof norm);
	if (!norm[0]) return;
	if (o && c && c - o < (int)sizeof region) snprintf(region, sizeof region, "%.*s", (int)(c - o + 1), o);
	/* the old short tags as No-Intro writes them: "(U)" is "(USA" */
	if (!strcmp(region, "(U)")) snprintf(region, sizeof region, "(USA");
	else if (!strcmp(region, "(E)")) snprintf(region, sizeof region, "(Europe");
	else if (!strcmp(region, "(J)")) snprintf(region, sizeof region, "(Japan");
	for (i = lower_bound(coll, ""); i < g.ne && g.e[i].coll == coll && g.nm < MAX_MATCH; i++) {
		entry *e = &g.e[i];

		if (!e->norm) {
			char nx[512], nn[512];

			entry_noext(e, nx, sizeof nx);
			art_norm(nx, nn, sizeof nn);
			e->norm = strdup(nn);
		}
		if (e->norm && !strcmp(e->norm, norm) && (!region[0] || strstr(e->base, region)))
			g.match[g.nm++] = i;
	}
}

static void match_game(const char *rom)
{
	g.nm = 0;
	match_exact(g.ca, rom);
	match_exact(g.cb, rom);
	if (!g.nm) match_loose(g.ca, rom);
	if (!g.nm) match_loose(g.cb, rom);
}

/* ---- a game's files ------------------------------------------------------------------ */

static unsigned file_crc(const char *path, bool *ok)
{
	size_t n;
	unsigned char *b = read_file(path, &n);
	unsigned c;

	*ok = b != NULL || access(path, F_OK) == 0;
	if (!b) return 0;
	c = (unsigned)crc32(0L, b, (uInt)n);
	free(b);
	return c;
}

/* Which of the game's entries are not on the card as they are in the
 * index; true when there is something to fetch */
static bool game_wants(void)
{
	char d[1024], p[1400];
	int i;
	bool any = false;

	game_dir(g.folder, g.rom, d, sizeof d);
	g.had = access(d, F_OK) == 0;
	for (i = 0; i < g.nm; i++) {
		bool ok;

		snprintf(p, sizeof p, "%s/%s", d, g.e[g.match[i]].base);
		g.want[i] = access(p, F_OK) != 0 || file_crc(p, &ok) != g.e[g.match[i]].crc;
		if (g.want[i]) any = true;
	}
	return any;
}

/* The game's folder holds its entries and nothing else: a file libretro
 * dropped goes, and .none */
static void game_tidy(bool none)
{
	char d[1024], p[1400];
	DIR *dp;
	struct dirent *de;

	game_dir(g.folder, g.rom, d, sizeof d);
	if (!(dp = opendir(d))) return;
	while ((de = readdir(dp))) {
		bool keep = false;
		int i;

		if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
		if (!strcmp(de->d_name, ".none")) keep = none;
		for (i = 0; !none && i < g.nm; i++) if (!strcmp(de->d_name, g.e[g.match[i]].base)) keep = true;
		if (keep) continue;
		snprintf(p, sizeof p, "%s/%s", d, de->d_name);
		remove(p);
	}
	closedir(dp);
}

static void game_none(void)
{
	char d[1024], p[1100];
	FILE *f;

	game_dir(g.folder, g.rom, d, sizeof d);
	snprintf(p, sizeof p, "%s/.none", d);
	if (access(p, F_OK) == 0) {
		/* checked before: none still, only files libretro dropped go */
		game_tidy(true);
		return;
	}
	mkdirs(d);
	game_tidy(true);
	if ((f = fopen(p, "w"))) fclose(f);
	g.p.none++;
}

/* A cheat file's count of cheats: its "cheats = N" line */
static int cheats_in(const unsigned char *b, size_t n)
{
	const char *p = (const char *)b, *end = p + n;

	while (p < end) {
		const char *nl = memchr(p, '\n', (size_t)(end - p));
		size_t l = nl ? (size_t)(nl - p) : (size_t)(end - p);

		if (l > 6 && !strncmp(p, "cheats", 6)) {
			const char *q = p + 6;

			while (q < p + l && (*q == ' ' || *q == '\t' || *q == '=')) q++;
			return atoi(q);
		}
		p += l + 1;
	}
	return 0;
}

/* An entry that came: its local header read, inflated, checked against the
 * index's checksum, written as .part and renamed. 1 written, 0 asked for
 * again with more room, -1 no good. */
static int entry_take(const entry *e, const unsigned char *b, size_t n)
{
	unsigned nlen, elen, data;
	unsigned char *out;
	char d[1024], p[1400], part[1420];
	FILE *f;
	bool ok = false;

	if (n < 30 || le32(b) != 0x04034b50) return -1;
	nlen = le16(b + 26);
	elen = le16(b + 28);
	data = 30 + nlen + elen;
	if ((size_t)data + e->csize > n) {
		if (g.range_extra) return -1;
		g.range_extra = (int)elen + (int)nlen + 64;
		return 0;
	}
	if (e->usize > ENTRY_MAX || e->csize > ENTRY_MAX || !(out = malloc((size_t)e->usize + 1))) return -1;
	if (e->method == 0 && e->csize == e->usize) {
		memcpy(out, b + data, e->usize);
		ok = true;
	} else if (e->method == 8) {
		z_stream z;

		memset(&z, 0, sizeof z);
		if (inflateInit2(&z, -MAX_WBITS) == Z_OK) {
			z.next_in = (Bytef *)(b + data);
			z.avail_in = e->csize;
			z.next_out = out;
			z.avail_out = e->usize;
			ok = inflate(&z, Z_FINISH) == Z_STREAM_END && z.total_out == e->usize;
			inflateEnd(&z);
		}
	}
	if (ok) ok = (unsigned)crc32(0L, out, e->usize) == e->crc;
	if (ok) {
		game_dir(g.folder, g.rom, d, sizeof d);
		mkdirs(d);
		snprintf(p, sizeof p, "%s/%s", d, e->base);
		snprintf(part, sizeof part, "%s.part", p);
		ok = (f = fopen(part, "wb")) && fwrite(out, 1, e->usize, f) == e->usize;
		if (f) ok = fclose(f) == 0 && ok;
		ok = ok && rename(part, p) == 0;
		if (!ok) remove(part);
		else g.got += cheats_in(out, e->usize);
	}
	free(out);
	return ok ? 1 : -1;
}

/* ---- asking ------------------------------------------------------------------------- */

static bool ask_range(unsigned long long from, unsigned long long to)
{
	char tmp[1024], h[96];

	tmp_path(tmp, sizeof tmp);
	if (to == ~0ULL) snprintf(h, sizeof h, "Range: bytes=-%llu", from);
	else snprintf(h, sizeof h, "Range: bytes=%llu-%llu", from, to);
	return net_get_async_hdr(CHT_URL, tmp, ASK_S, h);
}

/* A request's end: 1 the bytes are in the temporary, 0 still going, -1 it
 * failed (asked again later, or given up: g.problem) */
static int ask_poll(void)
{
	int r = net_async_poll(), http;

	if (r == 0) return 0;
	http = net_async_http();
	if (r == 1 && (http == 206 || http == 200)) {
		g.tries = 0;
		return 1;
	}
	fprintf(stderr, "cheats: no answer (http %d, curl %d)\n", http, net_async_exit());
	return -1;
}

/* A request that failed: again after 5, 15 and 30 s, then given up */
static bool retry_later(void)
{
	if (g.tries >= (int)(sizeof BACKOFF / sizeof BACKOFF[0])) { g.tries = 0; return false; }
	g.wait_until = plat_now_ms() + BACKOFF[g.tries++];
	g.p.state = CHT_RETRYING;
	g.p.resume_at = g.wait_until;
	return true;
}

/* ---- the run --------------------------------------------------------------------------- */

static void colls_add(const char *name)
{
	int i;

	if (!name) return;
	for (i = 0; i < g.ncoll; i++) if (!strcmp(g.coll[i], name)) return;
	if (g.ncoll < MAX_COLL) snprintf(g.coll[g.ncoll++], sizeof g.coll[0], "%s", name);
}

static int coll_index(const char *name)
{
	int i;

	for (i = 0; name && i < g.ncoll; i++) if (!strcmp(g.coll[i], name)) return i;
	return -1;
}

static void stop(int problem)
{
	g.problem = problem;
	g.p.problem = problem;
	g.phase = P_END;
}

static void begin_common(void)
{
	char root[1024];
	int i;

	root_path(root, sizeof root);
	mkdirs(root);
	g.ncoll = 0;
	for (i = 0; i < g.sys.count; i++) {
		colls_add(art_lr_collection(g.sys.systems[i].folder, 0));
		colls_add(art_lr_collection(g.sys.systems[i].folder, 1));
	}
	g.active = true;
	g.phase = P_TAIL;
	g.problem = CHT_OK;
	g.si = -1;
	g.tries = g.fail_row = 0;
}

bool cheats_begin(const systems_cfg *sys, const char *only, int games)
{
	int i;

	if (g.active) return false;
	memset(&g.p, 0, sizeof g.p);
	memset(&g.sys, 0, sizeof g.sys);
	g.one = false;
	snprintf(g.only, sizeof g.only, "%s", only ? only : "");
	for (i = 0; sys && i < sys->count; i++) {
		const system_cfg *s = &sys->systems[i];
		game_list l;

		if (g.only[0] && strcmp(s->folder, g.only)) continue;
		if (!cheats_mapped(s->folder) || g.sys.count >= CFG_MAX_SYSTEMS) continue;
		g.sys.systems[g.sys.count++] = *s;
		/* the games to look at, for the bar: the page counted them */
		if (games > 0) continue;
		memset(&l, 0, sizeof l);
		if (lib_scan_listing(P_ROMS, s->folder, s->exts, &l)) g.p.games += l.count;
		lib_free(&l);
	}
	if (games > 0) g.p.games = games;
	if (!g.sys.count) return false;
	begin_common();
	return true;
}

bool cheats_dl_one_begin(const char *folder, const char *rom_file)
{
	const char *b, *dot;
	char rom[LIB_NAME];

	if (g.active || !folder || !rom_file || !cheats_mapped(folder)) return false;
	b = strrchr(rom_file, '/');
	b = b ? b + 1 : rom_file;
	dot = strrchr(b, '.');
	snprintf(rom, sizeof rom, "%.*s", (int)(dot && dot > b ? dot - b : (long)strlen(b)), b);
	memset(&g.p, 0, sizeof g.p);
	memset(&g.sys, 0, sizeof g.sys);
	snprintf(g.sys.systems[0].folder, sizeof g.sys.systems[0].folder, "%s", folder);
	g.sys.count = 1;
	g.one = true;
	g.p.games = 1;
	snprintf(g.only, sizeof g.only, "%s", rom);
	begin_common();
	return true;
}

/* The next system's games, or the end */
static void next_system(void)
{
	lib_free(&g.games);
	memset(&g.games, 0, sizeof g.games);
	for (g.si++; g.si < g.sys.count; g.si++) {
		const system_cfg *s = &g.sys.systems[g.si];

		snprintf(g.folder, sizeof g.folder, "%s", s->folder);
		g.ca = coll_index(art_lr_collection(s->folder, 0));
		g.cb = coll_index(art_lr_collection(s->folder, 1));
		g.gi = 0;
		snprintf(g.p.folder_now, sizeof g.p.folder_now, "%s", s->folder);
		if (g.one) {
			/* one game: the name it was given */
			if (!(g.games.items = calloc(1, sizeof *g.games.items))) continue;
			snprintf(g.games.items[0].name, sizeof g.games.items[0].name, "%s", g.only);
			g.games.count = 1;
		} else if (!lib_scan_listing(P_ROMS, s->folder, s->exts, &g.games)) {
			continue;
		}
		g.phase = P_GAME;
		return;
	}
	g.phase = P_END;
}

/* A game looked at: its matches, and what of them it lacks */
static void game_look(void)
{
	if (g.gi >= g.games.count) { g.phase = P_SYSTEM; return; }
	snprintf(g.rom, sizeof g.rom, "%s", g.games.items[g.gi].name);
	snprintf(g.p.now, sizeof g.p.now, "%s", g.rom);
	{
		char d[1024];

		game_dir(g.folder, g.rom, d, sizeof d);
		g.had = access(d, F_OK) == 0;
	}
	match_game(g.rom);
	g.got = 0;
	g.range_extra = 0;
	if (!g.nm) {
		game_none();
		g.p.games_done++;
		g.gi++;
		return;
	}
	if (!game_wants()) {
		game_tidy(false);       /* a file libretro dropped goes */
		g.p.games_done++;
		g.gi++;
		return;
	}
	g.mi = 0;
	g.phase = P_FETCH;
}

/* The game's files are all in: it lands */
static void game_landed(void)
{
	game_tidy(false);
	if (g.had) g.p.updated++;
	else g.p.fetched++;
	snprintf(g.p.got_game, sizeof g.p.got_game, "%s", g.rom);
	snprintf(g.p.got_folder, sizeof g.p.got_folder, "%s", g.folder);
	g.p.got_count = g.got;
	g.p.seq++;
	g.p.games_done++;
	g.gi++;
	g.fail_row = 0;
	g.phase = P_GAME;
}

int cheats_step(void)
{
	char tmp[1024];
	unsigned char *b;
	size_t n;
	int r;

	if (!g.active) return -1;
	if (g.wait_until) {
		if (plat_now_ms() < g.wait_until) return 1;
		g.wait_until = 0;
		g.p.state = CHT_WORKING;
	}
	if (g.phase != P_END && !net_online()) {
		g.p.state = CHT_NO_NETWORK;
		return 1;
	}
	if (g.p.state == CHT_NO_NETWORK) g.p.state = CHT_WORKING;
	tmp_path(tmp, sizeof tmp);
	switch (g.phase) {
	case P_TAIL:
		snprintf(g.p.now, sizeof g.p.now, "%s", "cheats.zip");
		if (ask_range(TAIL, ~0ULL)) g.phase = P_TAIL_WAIT;
		else if (!retry_later()) stop(CHT_NO_ANSWER);
		return 1;
	case P_TAIL_WAIT:
		if ((r = ask_poll()) == 0) return 1;
		if (r < 0) { g.phase = P_TAIL; if (!retry_later()) stop(CHT_NO_ANSWER); return 1; }
		b = read_file(tmp, &n);
		if (!b || !tail_parse(b, n)) { free(b); stop(CHT_BAD_LIST); return 1; }
		free(b);
		remove(tmp);
		if (index_cached()) { g.phase = P_SYSTEM; return 1; }
		if (!ask_range(g.cd_off, (unsigned long long)g.cd_off + g.cd_size - 1)) {
			if (!retry_later()) stop(CHT_NO_ANSWER);
			g.phase = P_TAIL;
			return 1;
		}
		g.phase = P_CD_WAIT;
		return 1;
	case P_CD_WAIT: {
		char ip[1024];

		if ((r = ask_poll()) == 0) return 1;
		if (r < 0) { g.phase = P_TAIL; if (!retry_later()) stop(CHT_NO_ANSWER); return 1; }
		b = read_file(tmp, &n);
		if (!b || n != g.cd_size || !index_parse(b, n)) { free(b); stop(CHT_BAD_LIST); return 1; }
		free(b);
		index_path(ip, sizeof ip);
		if (rename(tmp, ip) == 0) index_keep();
		g.phase = P_SYSTEM;
		return 1;
	}
	case P_SYSTEM:
		next_system();
		return 1;
	case P_GAME:
		/* a few games a step while they need no request: a system already
		 * checked costs only reads, and a step stays short (it runs between
		 * a menu's frames) */
		for (int k = 0; k < 4 && g.phase == P_GAME; k++) {
			game_look();
			if (g.had) break;       /* its files were read for their checksums */
		}
		return 1;
	case P_FETCH: {
		const entry *e;

		while (g.mi < g.nm && !g.want[g.mi]) g.mi++;
		if (g.mi >= g.nm) { game_landed(); return 1; }
		e = &g.e[g.match[g.mi]];
		if (ask_range(e->off, (unsigned long long)e->off + 30 + (unsigned)e->nlen + LOCAL_ROOM + (unsigned)g.range_extra +
		                      e->csize - 1)) {
			g.phase = P_FETCH_WAIT;
		} else if (!retry_later()) {
			g.p.failed++;
			g.p.games_done++;
			g.gi++;
			g.phase = ++g.fail_row >= 5 ? P_END : P_GAME;
			if (g.fail_row >= 5) g.problem = g.p.problem = CHT_NO_ANSWER;
		}
		return 1;
	}
	case P_FETCH_WAIT:
		if ((r = ask_poll()) == 0) return 1;
		if (r < 0) {
			g.phase = P_FETCH;
			if (!retry_later()) {
				g.p.failed++;
				g.p.games_done++;
				g.gi++;
				g.phase = ++g.fail_row >= 5 ? P_END : P_GAME;
				if (g.fail_row >= 5) g.problem = g.p.problem = CHT_NO_ANSWER;
			}
			return 1;
		}
		b = read_file(tmp, &n);
		r = b ? entry_take(&g.e[g.match[g.mi]], b, n) : -1;
		free(b);
		remove(tmp);
		if (r == 0) { g.phase = P_FETCH; return 1; }       /* a longer header: again */
		if (r < 0) g.p.failed++;
		g.range_extra = 0;
		g.mi++;
		g.phase = P_FETCH;
		return 1;
	case P_END:
	default:
		lib_free(&g.games);
		memset(&g.games, 0, sizeof g.games);
		index_free();
		g.active = false;
		g.phase = P_IDLE;
		fprintf(stderr, "cheats: done: %d new, %d updated, %d with none, %d failed, problem %d\n", g.p.fetched,
		        g.p.updated, g.p.none, g.p.failed, g.problem);
		return g.problem == CHT_OK ? 0 : -1;
	}
}

void cheats_cancel(void)
{
	net_slot was;

	if (!g.active) return;
	was = net_slot_use(NET_SLOT_CHT);
	net_async_abort();
	net_slot_use(was);
	lib_free(&g.games);
	memset(&g.games, 0, sizeof g.games);
	index_free();
	g.active = false;
	g.phase = P_IDLE;
}

bool cheats_running(void) { return g.active; }

void cheats_status(cheats_progress *out) { *out = g.p; }

int cheats_dl_one_step(void)
{
	net_slot was;
	int r;

	if (!g.active || !g.one) return -1;
	was = net_slot_use(NET_SLOT_CHT);
	r = cheats_step();
	net_slot_use(was);
	return r == 1 ? 1 : r == 0 ? 0 : -1;
}

/* ---- deleting ------------------------------------------------------------------------- */

static bool key_collect(const char *key, const char *value, void *ctx)
{
	char ***v = ctx;
	size_t n = 0;

	(void)value;
	while (*v && (*v)[n]) n++;
	if (!(*v = realloc(*v, sizeof **v * (n + 2)))) return false;
	(*v)[n] = strdup(key);
	(*v)[n + 1] = NULL;
	return true;
}

void cheats_delete_sys(const char *folder)
{
	char d[1024];

	libretro_dir(folder, d, sizeof d);
	rm_tree(d);
}

void cheats_delete_all(const systems_cfg *sys)
{
	char **keys = NULL, ip[1024], kp[1024];
	int i;

	for (i = 0; sys && i < sys->count; i++) cheats_delete_sys(sys->systems[i].folder);
	index_path(ip, sizeof ip);
	key_path(kp, sizeof kp);
	remove(ip);
	remove(kp);
	/* the cheats that were on turn off: the in-game list's switches */
	db_each_prefix(db_lib(), "cheats.", key_collect, &keys);
	for (i = 0; keys && keys[i]; i++) {
		db_del(db_lib(), keys[i]);
		free(keys[i]);
	}
	free(keys);
}
