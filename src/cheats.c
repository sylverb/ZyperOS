/* SPDX-License-Identifier: GPL-3.0-only */
/* See cheats.h. */
#include "cheats.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "atomic.h"

/* A .cht larger is no cheat file: libretro's largest is under 200 KB */
#define CHEATS_FILE_MAX (4u << 20)

uint32_t cheats_id(const char *file, const char *desc, const char *code)
{
	const char *parts[3] = { file ? file : "", desc ? desc : "", code ? code : "" };
	uint32_t h = 2166136261u;
	int p;

	for (p = 0; p < 3; p++) {
		const unsigned char *s = (const unsigned char *)parts[p];

		for (; *s; s++) h = (h ^ *s) * 16777619u;
		h = (h ^ 0xffu) * 16777619u;   /* between the parts */
	}
	return h;
}

void cheats_key(const char *tag, const char *file, char *out, size_t n)
{
	snprintf(out, n, "cheats.%s.%s", tag ? tag : "", file ? file : "");
}

void cheats_asked_key(const char *tag, const char *file, char *out, size_t n)
{
	snprintf(out, n, "cheats-asked.%s.%s", tag ? tag : "", file ? file : "");
}

void cheats_free(cheat_list *L)
{
	int i;

	for (i = 0; i < L->n; i++) {
		free(L->c[i].desc);
		free(L->c[i].code);
	}
	L->n = L->ng = 0;
}

typedef struct {
	cheat_list *L;
	const char *file;   /* the group's file name, for the ids */
	int group;
} add_ctx;

static void add_one(const char *desc, const char *code, void *ctx_)
{
	add_ctx *x = ctx_;
	cheat *c;

	if (x->L->n >= CHEATS_MAX) return;
	c = &x->L->c[x->L->n];
	c->desc = strdup(desc);
	c->code = strdup(code);
	if (!c->desc || !c->code) {
		free(c->desc);
		free(c->code);
		return;
	}
	c->id = cheats_id(x->file, desc, code);
	c->group = (uint8_t)x->group;
	c->on = false;
	x->L->n++;
}

/* One file's cheats as a group, when it has any */
static void add_file(cheat_list *L, const char *path, const char *name,
                     bool yours, const char *label)
{
	struct stat st;
	char *text;
	FILE *f;
	size_t got;
	add_ctx x;
	int before = L->n;

	if (L->ng >= CHEATS_GROUPS) return;
	if (stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
	    (unsigned long long)st.st_size > CHEATS_FILE_MAX)
		return;
	f = fopen(path, "rb");
	if (!f) return;
	text = malloc((size_t)st.st_size);
	got = text ? fread(text, 1, (size_t)st.st_size, f) : 0;
	fclose(f);
	if (!text) return;

	x.L = L;
	x.file = name;
	/* The player's own files are one group, whichever names they have */
	x.group = yours && L->ng > 0 && L->g[L->ng - 1].yours ? L->ng - 1 : L->ng;
	cht_parse(text, got, add_one, &x);
	free(text);
	if (L->n == before || x.group < L->ng) return;
	snprintf(L->g[L->ng].label, sizeof L->g[L->ng].label, "%s", label ? label : "");
	L->g[L->ng].yours = yours;
	L->ng++;
}

/* The game's name: its file's without the extension */
static void stem(const char *file, char *out, size_t n)
{
	const char *slash = strrchr(file, '/');
	char *dot;

	snprintf(out, n, "%s", slash ? slash + 1 : file);
	dot = strrchr(out, '.');
	if (dot && dot != out) *dot = '\0';
}

/* By name without ".cht", so the game's own file comes before the ones
 * named for a code type after it ("X (USA)" before "X (USA) (Game Genie)",
 * where the whole names would sort the space before the dot) */
static int by_name(const void *a, const void *b)
{
	const char *x = *(char *const *)a, *y = *(char *const *)b;
	size_t xl = strlen(x) - 4, yl = strlen(y) - 4;
	int c = strncmp(x, y, xl < yl ? xl : yl);

	return c ? c : xl < yl ? -1 : xl > yl;
}

/* A libretro file's code type, the part of its name after the game's: "Super
 * Mario World (USA) (Game Genie).cht" beside "Super Mario World (USA).cht"
 * is "Game Genie", its brackets off. The game's own name is the shortest of
 * the folder's; a file that does not begin with it is named whole. */
static void code_type(const char *base, const char *game, char *out, size_t n)
{
	size_t gl = strlen(game);
	const char *r = base;
	size_t rl;

	if (gl && !strncmp(base, game, gl)) r = base + gl;
	while (*r == ' ' || *r == '-' || *r == '_') r++;
	rl = strlen(r);
	if (rl >= 2 && r[0] == '(' && r[rl - 1] == ')' && !memchr(r + 1, '(', rl - 2)) {
		r++;
		rl -= 2;
	}
	snprintf(out, n, "%.*s", (int)rl, r);
}

static bool ends_cht(const char *name)
{
	size_t l = strlen(name);

	return l > 4 && !strcasecmp(name + l - 4, ".cht");
}

/* libretro's files for the game, a group each, in name order */
static void add_libretro(cheat_list *L, const char *dir, bool *none)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char *names[CHEATS_GROUPS];
	char game[CHEATS_LABEL * 2] = "";
	int n = 0, i;

	if (!d) return;
	while ((e = readdir(d)) && n < CHEATS_GROUPS) {
		if (!strcmp(e->d_name, ".none")) { *none = true; continue; }
		if (e->d_name[0] == '.' || !ends_cht(e->d_name)) continue;
		names[n] = strdup(e->d_name);
		if (names[n]) n++;
	}
	closedir(d);
	qsort(names, (size_t)n, sizeof *names, by_name);
	for (i = 0; i < n; i++) {
		char base[CHEATS_LABEL * 2];

		snprintf(base, sizeof base, "%.*s", (int)(strlen(names[i]) - 4), names[i]);
		if (!game[0] || strlen(base) < strlen(game)) snprintf(game, sizeof game, "%s", base);
	}
	for (i = 0; i < n; i++) {
		char base[CHEATS_LABEL * 2], label[CHEATS_LABEL], path[2048];

		snprintf(base, sizeof base, "%.*s", (int)(strlen(names[i]) - 4), names[i]);
		code_type(base, game, label, sizeof label);
		snprintf(path, sizeof path, "%s/%s", dir, names[i]);
		add_file(L, path, names[i], false, label);
		free(names[i]);
	}
}

bool cheats_load(cheat_list *L, const char *card, const char *folder,
                 const char *tag, const char *file)
{
	char name[1024], path[2048], seen[4][2048];
	const char *dirs[2] = { folder, tag };
	int nseen = 0, di, k, i;
	bool none = false;

	memset(L, 0, sizeof *L);
	L->where = CHEATS_NOT_CHECKED;
	if (!card || !folder || !file || !*file) return false;
	stem(file, name, sizeof name);

	/* The player's own, in the folder, then in NextUI's: each name once */
	for (di = 0; di < 2; di++) {
		const char *names[2] = { name, file };

		if (!dirs[di] || !*dirs[di] || (di == 1 && !strcmp(dirs[1], dirs[0]))) continue;
		for (k = 0; k < 2; k++) {
			const char *slash = strrchr(names[k], '/');
			bool dup = false;

			snprintf(path, sizeof path, "%s/Cheats/%s/%s.cht", card, dirs[di],
			         slash ? slash + 1 : names[k]);
			for (i = 0; i < nseen; i++) dup = dup || !strcmp(seen[i], path);
			if (dup || nseen >= 4) continue;
			snprintf(seen[nseen++], sizeof seen[0], "%s", path);
			add_file(L, path, strrchr(path, '/') + 1, true, "");
		}
	}

	snprintf(path, sizeof path, "%s/Cheats/%s/.libretro/%s", card, folder, name);
	add_libretro(L, path, &none);

	L->where = L->n ? CHEATS_HAVE : none ? CHEATS_NONE_LIBRETRO : CHEATS_NOT_CHECKED;
	return L->n > 0;
}

void cheats_state_apply(cheat_list *L, const char *state)
{
	const char *s = state ? state : "";
	int i;

	for (i = 0; i < L->n; i++) L->c[i].on = false;
	while (*s) {
		char *end;
		unsigned long id = strtoul(s, &end, 16);

		if (end == s) { s++; continue; }
		for (i = 0; i < L->n; i++)
			if (L->c[i].id == (uint32_t)id) L->c[i].on = true;
		s = end;
	}
}

void cheats_state_str(const cheat_list *L, char *out, size_t n)
{
	size_t at = 0;
	int i;

	if (!n) return;
	out[0] = '\0';
	for (i = 0; i < L->n; i++) {
		int w;

		if (!L->c[i].on) continue;
		w = snprintf(out + at, n - at, "%s%08x", at ? "," : "", (unsigned)L->c[i].id);
		if (w < 0 || (size_t)w >= n - at) {   /* no room: the ones that fit */
			out[at] = '\0';
			return;
		}
		at += (size_t)w;
	}
}

int cheats_on_count(const cheat_list *L)
{
	int i, n = 0;

	for (i = 0; i < L->n; i++) n += L->c[i].on;
	return n;
}

int cheats_write_codes(const cheat_list *L, const char *path)
{
	FILE *f = atomic_open(path, 0644);
	int i, n = 0;

	if (!f) return -1;
	for (i = 0; i < L->n; i++) {
		if (!L->c[i].on) continue;
		fprintf(f, "%s\n", L->c[i].code);
		n++;
	}
	return atomic_commit(f, path) ? n : -1;
}

int cheats_order(const cheat_list *L, bool on_first, int *order)
{
	int i, n = 0, pass;

	for (pass = 0; pass < (on_first ? 2 : 1); pass++)
		for (i = 0; i < L->n; i++)
			if (!on_first || L->c[i].on == (pass == 0)) order[n++] = i;
	return n;
}
