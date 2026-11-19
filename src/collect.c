/* SPDX-License-Identifier: GPL-3.0-only */
/* Collections on the card. See src/collect.h. */
#include "collect.h"

#include "atomic.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
	char name[COLL_NAME];
	char **line;          /* as read, its line end dropped */
	int n, cap;
} coll;

static coll g_coll[COLL_MAX];
static int g_n;
static char g_card[512];

static void coll_drop(coll *c)
{
	for (int k = 0; k < c->n; k++) free(c->line[k]);
	free(c->line);
	memset(c, 0, sizeof *c);
}

void coll_free(void)
{
	for (int i = 0; i < g_n; i++) coll_drop(&g_coll[i]);
	g_n = 0;
}

static bool push_line(coll *c, const char *s, size_t len)
{
	char *copy;

	if (c->n == c->cap) {
		int cap = c->cap ? c->cap * 2 : 16;
		char **more = realloc(c->line, (size_t)cap * sizeof *more);

		if (!more) return false;
		c->line = more;
		c->cap = cap;
	}
	if (!(copy = malloc(len + 1))) return false;
	memcpy(copy, s, len);
	copy[len] = '\0';
	c->line[c->n++] = copy;
	return true;
}

static void dir_path(char *out, size_t n)
{
	snprintf(out, n, "%s/%s", g_card, COLL_DIR);
}

static bool file_path(const char *name, char *out, size_t n)
{
	return snprintf(out, n, "%s/%s/%s.txt", g_card, COLL_DIR, name) < (int)n;
}

static int by_name(const void *pa, const void *pb)
{
	const coll *a = pa, *b = pb;
	int c = strcasecmp(a->name, b->name);

	return c ? c : strcmp(a->name, b->name);
}

/* A .txt the card holds, by its stem; a dot file (macOS's ._ shadows) is
 * none */
static bool list_name(const char *d_name, char *stem, size_t n)
{
	size_t len = strlen(d_name);

	if (d_name[0] == '.' || len <= 4 || strcasecmp(d_name + len - 4, ".txt")) return false;
	if (len - 4 >= n) return false;
	snprintf(stem, n, "%.*s", (int)(len - 4), d_name);
	return true;
}

static bool read_list(coll *c, const char *path)
{
	FILE *f = fopen(path, "rb");
	char *buf = NULL;
	size_t cap = 0;
	ssize_t got;
	bool first = true;

	if (!f) return false;
	while ((got = getline(&buf, &cap, f)) >= 0) {
		const char *s = buf;
		size_t len = (size_t)got;

		/* A list saved with a BOM by a text editor */
		if (first && len >= 3 && !memcmp(s, "\xEF\xBB\xBF", 3)) { s += 3; len -= 3; }
		first = false;
		while (len && (s[len - 1] == '\n' || s[len - 1] == '\r')) len--;
		if (!push_line(c, s, len)) break;
	}
	free(buf);
	fclose(f);
	/* A last empty line is the file's own end, not a line of its own */
	while (c->n && !c->line[c->n - 1][0]) free(c->line[--c->n]);
	return true;
}

void coll_load(const char *card)
{
	char dir[600], path[800], stem[COLL_NAME];
	struct dirent *e;
	DIR *d;

	coll_free();
	snprintf(g_card, sizeof g_card, "%s", card ? card : "");
	dir_path(dir, sizeof dir);
	if (!(d = opendir(dir))) return;
	while ((e = readdir(d)) && g_n < COLL_MAX) {
		coll *c = &g_coll[g_n];

		if (!list_name(e->d_name, stem, sizeof stem)) continue;
		memset(c, 0, sizeof *c);
		snprintf(c->name, sizeof c->name, "%s", stem);
		if (!file_path(stem, path, sizeof path) || !read_list(c, path)) {
			coll_drop(c);
			continue;
		}
		g_n++;
	}
	closedir(d);
	qsort(g_coll, (size_t)g_n, sizeof *g_coll, by_name);
}

int coll_count(void)
{
	return g_n;
}

const char *coll_name(int i)
{
	return i >= 0 && i < g_n ? g_coll[i].name : "";
}

int coll_find(const char *name)
{
	for (int i = 0; name && i < g_n; i++)
		if (!strcasecmp(g_coll[i].name, name)) return i;
	return -1;
}

int coll_lines(int i)
{
	return i >= 0 && i < g_n ? g_coll[i].n : 0;
}

bool coll_parse(const char *line, char *folder, size_t fn, char *file, size_t n)
{
	char buf[COLL_LINE];
	const char *p, *slash;
	size_t len;

	if (!line) return false;
	while (*line == ' ' || *line == '\t') line++;
	if (!*line || *line == '#') return false;
	if ((len = strlen(line)) >= sizeof buf) return false;
	memcpy(buf, line, len + 1);
	while (len && (buf[len - 1] == ' ' || buf[len - 1] == '\t')) buf[--len] = '\0';
	for (char *q = buf; *q; q++)
		if (*q == '\\') *q = '/';
	p = buf;
	if (!strncmp(p, "/Roms/", 6)) p += 6;
	else if (!strncmp(p, "Roms/", 5)) p += 5;
	else if ((slash = strstr(p, "/Roms/"))) p = slash + 6;
	while (*p == '/') p++;
	if (!(slash = strchr(p, '/')) || slash == p || !slash[1]) return false;
	if ((size_t)(slash - p) >= fn || strlen(slash + 1) >= n) return false;
	snprintf(folder, fn, "%.*s", (int)(slash - p), p);
	snprintf(file, n, "%s", slash + 1);
	return true;
}

void coll_line_for(const char *folder, const char *file, char *out, size_t n)
{
	snprintf(out, n, "/Roms/%s/%s", folder ? folder : "", file ? file : "");
}

bool coll_game(int i, int k, char *folder, size_t fn, char *file, size_t n)
{
	if (i < 0 || i >= g_n || k < 0 || k >= g_coll[i].n) return false;
	return coll_parse(g_coll[i].line[k], folder, fn, file, n);
}

int coll_games(int i)
{
	char folder[COLL_LINE], file[COLL_LINE];
	int count = 0;

	for (int k = 0; k < coll_lines(i); k++)
		count += coll_game(i, k, folder, sizeof folder, file, sizeof file);
	return count;
}

/* Line k of c names this game: its folder in any case, as a card's file
 * names are; its path as it is */
static bool line_is(const coll *c, int k, const char *folder, const char *file)
{
	char f[COLL_LINE], p[COLL_LINE];

	return coll_parse(c->line[k], f, sizeof f, p, sizeof p) &&
	       !strcasecmp(f, folder) && !strcmp(p, file);
}

bool coll_has(int i, const char *folder, const char *file)
{
	if (i < 0 || i >= g_n || !folder || !file) return false;
	for (int k = 0; k < g_coll[i].n; k++)
		if (line_is(&g_coll[i], k, folder, file)) return true;
	return false;
}

static bool write_list(const coll *c)
{
	char dir[600], path[800];
	FILE *f;

	dir_path(dir, sizeof dir);
	if (mkdir(dir, 0755) != 0 && errno != EEXIST) return false;
	if (!file_path(c->name, path, sizeof path) || !(f = atomic_open(path, 0644))) return false;
	for (int k = 0; k < c->n; k++)
		if (fprintf(f, "%s\n", c->line[k]) < 0) { atomic_abort(f, path); return false; }
	return atomic_commit(f, path);
}

bool coll_add(int i, const char *folder, const char *file)
{
	char line[COLL_LINE];
	coll *c;

	if (i < 0 || i >= g_n || !folder || !*folder || !file || !*file) return false;
	c = &g_coll[i];
	if (coll_has(i, folder, file)) return true;
	if (c->n >= COLL_LINES) return false;
	coll_line_for(folder, file, line, sizeof line);
	if (strlen(line) >= sizeof line - 1 || !push_line(c, line, strlen(line))) return false;
	if (write_list(c)) return true;
	free(c->line[--c->n]);
	return false;
}

/* The lines naming the game taken out of c, in memory; how many */
static int drop_game(coll *c, const char *folder, const char *file)
{
	int k, w = 0, gone = 0;

	for (k = 0; k < c->n; k++) {
		if (line_is(c, k, folder, file)) { free(c->line[k]); gone++; continue; }
		c->line[w++] = c->line[k];
	}
	c->n = w;
	return gone;
}

bool coll_remove(int i, const char *folder, const char *file)
{
	if (i < 0 || i >= g_n || !folder || !file) return false;
	if (!drop_game(&g_coll[i], folder, file)) return true;
	return write_list(&g_coll[i]);
}

/* `name` with the spaces at its ends dropped, into out */
static void trimmed(const char *name, char *out, size_t n)
{
	size_t len;

	while (*name == ' ' || *name == '\t') name++;
	snprintf(out, n, "%s", name);
	len = strlen(out);
	while (len && (out[len - 1] == ' ' || out[len - 1] == '\t')) out[--len] = '\0';
}

coll_name_err coll_name_check(const char *name, int self)
{
	char t[COLL_NAME * 2];
	int at;

	if (!name) return COLL_NAME_EMPTY;
	trimmed(name, t, sizeof t);
	if (!t[0]) return COLL_NAME_EMPTY;
	if (strlen(t) >= COLL_NAME) return COLL_NAME_LONG;
	/* A dot first hides the file on the card (and ._ is macOS's own) */
	if (t[0] == '.') return COLL_NAME_BAD;
	for (const unsigned char *p = (const unsigned char *)t; *p; p++)
		if (*p < 0x20 || *p == 0x7f || strchr("/\\:*?\"<>|", *p)) return COLL_NAME_BAD;
	/* A dot last is dropped by Windows and by exFAT's tools */
	if (t[strlen(t) - 1] == '.') return COLL_NAME_BAD;
	at = coll_find(t);
	if (at >= 0 && at != self) return COLL_NAME_TAKEN;
	return COLL_NAME_OK;
}

/* Collection named `name` after a change of its place in the order */
static int index_of(const char *name)
{
	for (int i = 0; i < g_n; i++)
		if (!strcmp(g_coll[i].name, name)) return i;
	return -1;
}

int coll_create(const char *name, const char *folder, const char *file)
{
	char t[COLL_NAME * 2], line[COLL_LINE];
	coll *c;

	if (g_n >= COLL_MAX || coll_name_check(name, -1) != COLL_NAME_OK) return -1;
	if (!folder || !*folder || !file || !*file) return -1;
	trimmed(name, t, sizeof t);
	c = &g_coll[g_n];
	memset(c, 0, sizeof *c);
	snprintf(c->name, sizeof c->name, "%s", t);
	coll_line_for(folder, file, line, sizeof line);
	if (!push_line(c, line, strlen(line)) || !write_list(c)) {
		coll_drop(c);
		return -1;
	}
	g_n++;
	qsort(g_coll, (size_t)g_n, sizeof *g_coll, by_name);
	return index_of(t);
}

int coll_rename(int i, const char *name)
{
	char t[COLL_NAME * 2], from[800], to[800];

	if (i < 0 || i >= g_n || coll_name_check(name, i) != COLL_NAME_OK) return -1;
	trimmed(name, t, sizeof t);
	if (!strcmp(t, g_coll[i].name)) return i;
	if (!file_path(g_coll[i].name, from, sizeof from) || !file_path(t, to, sizeof to)) return -1;
	if (rename(from, to) != 0) return -1;
	snprintf(g_coll[i].name, sizeof g_coll[i].name, "%s", t);
	qsort(g_coll, (size_t)g_n, sizeof *g_coll, by_name);
	return index_of(t);
}

bool coll_delete(int i)
{
	char path[800];

	if (i < 0 || i >= g_n || !file_path(g_coll[i].name, path, sizeof path)) return false;
	if (unlink(path) != 0 && errno != ENOENT) return false;
	coll_drop(&g_coll[i]);
	memmove(&g_coll[i], &g_coll[i + 1], (size_t)(g_n - i - 1) * sizeof *g_coll);
	g_n--;
	memset(&g_coll[g_n], 0, sizeof g_coll[g_n]);
	return true;
}

int coll_follow(const char *folder, const char *file, const char *new_file)
{
	char f[COLL_LINE], p[COLL_LINE], line[COLL_LINE];
	int changed = 0;

	if (!folder || !file || !new_file || !strcmp(file, new_file)) return 0;
	for (int i = 0; i < g_n; i++) {
		coll *c = &g_coll[i];
		bool any = false;

		for (int k = 0; k < c->n; k++) {
			char *copy;

			if (!coll_parse(c->line[k], f, sizeof f, p, sizeof p) ||
			    strcasecmp(f, folder) || strcmp(p, file))
				continue;
			coll_line_for(f, new_file, line, sizeof line);
			if (!(copy = strdup(line))) continue;
			free(c->line[k]);
			c->line[k] = copy;
			any = true;
		}
		if (any && write_list(c)) changed++;
	}
	return changed;
}

int coll_forget(const char *folder, const char *file)
{
	int changed = 0;

	if (!folder || !file) return 0;
	for (int i = 0; i < g_n; i++)
		if (drop_game(&g_coll[i], folder, file) && write_list(&g_coll[i])) changed++;
	return changed;
}
