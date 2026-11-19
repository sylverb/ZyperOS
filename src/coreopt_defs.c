/* SPDX-License-Identifier: GPL-3.0-only */
/* See src/coreopt_defs.h. */
#include "coreopt_defs.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *field(const char *line, const char *name)
{
	char key[48];
	const char *p;

	snprintf(key, sizeof key, "%s=", name);
	/* Diatom's first field sits right after OPTION\t */
	if (!strncmp(line, "OPTION\t", 7) && !strncmp(line + 7, key, strlen(key)))
		return line + 7 + strlen(key);
	snprintf(key, sizeof key, "\t%s=", name);
	p = strstr(line, key);
	return p ? p + strlen(key) : NULL;
}

static void copy_field(char *out, size_t n, const char *line, const char *name)
{
	const char *p = field(line, name);
	size_t i = 0;

	out[0] = '\0';
	if (!p || n == 0) return;
	while (p[i] && p[i] != '\t' && p[i] != '\r' && p[i] != '\n' && i + 1 < n) {
		out[i] = p[i];
		i++;
	}
	out[i] = '\0';
}

void codef_set_values(codef *o, const char *pipe)
{
	const char *p;
	int n = 0;

	o->nvals = 0;
	o->pipe[0] = '\0';
	if (!pipe) return;
	snprintf(o->pipe, sizeof o->pipe, "%s", pipe);
	p = o->pipe;
	while (*p && n < CODEFS_VALUES_MAX) {
		size_t i = 0, copy;

		while (p[i] && p[i] != '|') i++;
		/* Truncate what we store, but still skip the whole field: cutting
		 * `i` and advancing only that far used to turn the tail into a
		 * second bogus value. Past CODEFS_VALUES_MAX, the rest is dropped. */
		copy = i < CODEFS_VAL_MAX ? i : CODEFS_VAL_MAX - 1;
		memcpy(o->vals[n], p, copy);
		o->vals[n][copy] = '\0';
		if (o->vals[n][0]) n++;
		p += i;
		if (*p == '|') p++;
	}
	o->nvals = n;
}

bool codef_parse_line(const char *line, codef *o)
{
	char vals[CODEFS_PIPE_MAX];

	if (!line || !o) return false;
	if (strncmp(line, "OPTION\t", 7) != 0) return false;
	memset(o, 0, sizeof *o);
	copy_field(o->key, sizeof o->key, line, "key");
	if (!o->key[0]) return false;
	copy_field(o->value, sizeof o->value, line, "value");
	copy_field(o->deflt, sizeof o->deflt, line, "default");
	copy_field(o->desc, sizeof o->desc, line, "desc");
	copy_field(vals, sizeof vals, line, "values");
	codef_set_values(o, vals);
	return true;
}

bool codefs_load_file(codefs *out, const char *path)
{
	FILE *f;
	char line[1024];

	if (!out) return false;
	out->n = 0;
	if (!path || !path[0]) return false;
	f = fopen(path, "r");
	if (!f) return false;
	while (fgets(line, sizeof line, f)) {
		codef o;

		line[strcspn(line, "\r\n")] = '\0';
		if (!codef_parse_line(line, &o)) continue;
		if (out->n >= CODEFS_MAX) break;
		out->opt[out->n++] = o;
	}
	fclose(f);
	return out->n > 0;
}

bool codefs_save_file(const codefs *in, const char *path)
{
	FILE *f;
	int i;

	if (!in || !path || !path[0]) return false;
	f = fopen(path, "w");
	if (!f) return false;
	for (i = 0; i < in->n; i++) {
		const codef *o = &in->opt[i];

		fprintf(f, "OPTION\tkey=%s\tvalue=%s\tdefault=%s\tvalues=%s\tdesc=%s\n",
		        o->key, o->value, o->deflt, o->pipe, o->desc);
	}
	fclose(f);
	return true;
}

void codefs_cache_path(const char *userdata, const char *core,
                       char *out, size_t n)
{
	if (!out || n == 0) return;
	out[0] = '\0';
	if (!userdata || !core || !core[0]) return;
	snprintf(out, n, "%s/coreopt-defs/%s.txt", userdata, core);
}

void codefs_cache_clear(const char *userdata)
{
	char dir[512], path[512];
	DIR *d;
	struct dirent *e;

	if (!userdata || !userdata[0]) return;
	snprintf(dir, sizeof dir, "%s/coreopt-defs", userdata);
	d = opendir(dir);
	if (!d) return;
	while ((e = readdir(d))) {
		size_t n = strlen(e->d_name);

		if (n < 5 || strcmp(e->d_name + n - 4, ".txt") != 0) continue;
		snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
		unlink(path);
	}
	closedir(d);
}

bool codefs_seed(codefs *out, const char *core)
{
	/* No baked-in defs: Core Settings uses Diatom's OPTIONS or the
	 * userdata cache only. A blueMSX seed lived here so MSX mapper /
	 * machine type were editable from the shelf before a first launch;
	 * removed to check whether that path matters in practice. */
	(void)core;
	if (out) out->n = 0;
	return false;
}

int codefs_load_allow(const char *path, const char *core,
                      char allow[][CODEFS_KEY_MAX], int max)
{
	FILE *f;
	char line[256];
	char cur[64];
	int n = 0;
	bool ours = false, saw = false;

	if (!allow || max <= 0 || !core || !core[0]) return 0;
	if (!path || !path[0]) return 0;
	f = fopen(path, "r");
	if (!f) return 0;
	cur[0] = '\0';
	while (fgets(line, sizeof line, f)) {
		char *p = line;

		line[strcspn(line, "\r\n")] = '\0';
		while (*p == ' ' || *p == '\t') p++;
		if (!*p || *p == '#') continue;
		if (!strncmp(p, "core=", 5)) {
			snprintf(cur, sizeof cur, "%s", p + 5);
			ours = strcmp(cur, core) == 0;
			if (ours) saw = true;
			continue;
		}
		if (!ours || strncmp(p, "key=", 4) != 0) continue;
		if (n >= max) break;
		snprintf(allow[n], CODEFS_KEY_MAX, "%s", p + 4);
		if (allow[n][0]) n++;
	}
	fclose(f);
	(void)saw;
	return n;
}

void codefs_filter(const codefs *in, codefs *out,
                   const char allow[][CODEFS_KEY_MAX], int n_allow)
{
	int i, k;

	if (!out) return;
	out->n = 0;
	if (!in) return;
	if (n_allow <= 0) {
		*out = *in;
		return;
	}
	for (i = 0; i < in->n; i++) {
		bool ok = false;

		for (k = 0; k < n_allow; k++)
			if (!strcmp(in->opt[i].key, allow[k])) { ok = true; break; }
		if (!ok) continue;
		if (out->n >= CODEFS_MAX) break;
		out->opt[out->n++] = in->opt[i];
	}
}

int codef_value_index(const codef *o, const char *value)
{
	int i;

	if (!o || !value) return -1;
	for (i = 0; i < o->nvals; i++)
		if (!strcmp(o->vals[i], value)) return i;
	return -1;
}

int codef_value_step(const codef *o, int cur, int dir)
{
	int n;

	if (!o || o->nvals <= 0) return 0;
	n = o->nvals;
	if (cur < 0 || cur >= n) cur = 0;
	return (cur + (dir < 0 ? -1 : 1) + n) % n;
}

void codef_db_key(const char *tag, const char *option, char *out, size_t n)
{
	if (!out || n == 0) return;
	out[0] = '\0';
	if (!tag || !option) return;
	snprintf(out, n, "coreopt.%s.%s", tag, option);
}
