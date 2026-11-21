/* SPDX-License-Identifier: GPL-3.0-only */
/* Your PICO-8's Shader and Border: the look file and the shader list. See
 * src/p8fx.h. */
#include "p8fx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* STRENGTH_MIN and STRENGTH_MAX alone: the shim links no database */
#include "strength.h"

/* The ones your PICO-8 is offered, in SHADERS' order (src/app.h), each
 * with its words and whether only a GPU draws it */
static const struct { const char *name; str_id text; bool gpu; } LIST[] = {
	{ "nearest",     S_MENU_OFF_TITLE,              false },
	{ "shimmerless", S_GAMEMENU_SHADER_SHIMMERLESS, false },
	{ "crt",         S_GAMEMENU_SHADER_CRT,         true },
	{ "crt-curved",  S_GAMEMENU_SHADER_CRT_CURVED,  true },
	{ "scanlines",   S_GAMEMENU_SHADER_SCANLINES,   true },
	{ "lcd",         S_GAMEMENU_SHADER_LCD,         true },
};
#define LIST_N ((int)(sizeof LIST / sizeof LIST[0]))

static const char *const BORDER_WORD[] = { "off", "blur", "picture" };

void p8fx_look_default(p8fx_look *l)
{
	memset(l, 0, sizeof *l);
	snprintf(l->shader, sizeof l->shader, "%s", LIST[0].name);
	l->strength = STRENGTH_MAX;
	l->border = P8FX_BORDER_OFF;
}

const char *p8fx_border_word(p8fx_border b)
{
	return b >= P8FX_BORDER_OFF && b <= P8FX_BORDER_PICTURE ? BORDER_WORD[b] : BORDER_WORD[0];
}

p8fx_border p8fx_border_parse(const char *word)
{
	int i;

	for (i = 0; word && i < 3; i++)
		if (!strcmp(word, BORDER_WORD[i])) return (p8fx_border)i;
	return P8FX_BORDER_OFF;
}

bool p8fx_look_read(const char *path, p8fx_look *l)
{
	char line[320];
	FILE *f;

	p8fx_look_default(l);
	if (!path || !(f = fopen(path, "r"))) return false;
	while (fgets(line, sizeof line, f)) {
		char *v = strchr(line, ' '), *e;

		if (!v) continue;
		*v++ = '\0';
		e = v + strcspn(v, "\r\n");
		*e = '\0';
		if (!strcmp(line, "shader")) {
			if (strlen(v) < sizeof l->shader) snprintf(l->shader, sizeof l->shader, "%s", v);
		} else if (!strcmp(line, "strength")) {
			long pct = strtol(v, &e, 10);

			if (e != v && pct >= STRENGTH_MIN && pct <= STRENGTH_MAX) l->strength = (int)pct;
		} else if (!strcmp(line, "border")) {
			l->border = p8fx_border_parse(v);
		} else if (!strcmp(line, "border_file")) {
			if (strlen(v) < sizeof l->border_file) snprintf(l->border_file, sizeof l->border_file, "%s", v);
		}
	}
	fclose(f);
	return true;
}

bool p8fx_look_write(const char *path, const p8fx_look *l)
{
	char tmp[512];
	FILE *f;
	bool ok;

	/* Strength as the reader takes it: 100 for one never set */
	int pct = l->strength < STRENGTH_MIN || l->strength > STRENGTH_MAX ? STRENGTH_MAX : l->strength;

	if (!path || snprintf(tmp, sizeof tmp, "%s.new", path) >= (int)sizeof tmp) return false;
	if (!(f = fopen(tmp, "w"))) return false;
	ok = fprintf(f, "shader %s\nstrength %d\nborder %s\n", l->shader[0] ? l->shader : LIST[0].name,
	             pct, p8fx_border_word(l->border)) > 0;
	if (ok && l->border_file[0]) ok = fprintf(f, "border_file %s\n", l->border_file) > 0;
	ok = fclose(f) == 0 && ok;
	if (!ok || rename(tmp, path) != 0) {
		unlink(tmp);
		return false;
	}
	return true;
}

bool p8fx_look_on(const p8fx_look *l)
{
	return strcmp(l->shader, LIST[0].name) != 0 || l->border != P8FX_BORDER_OFF;
}

int p8fx_shaders_on(bool gpu, const char **names, int max)
{
	int i, n = 0;

	for (i = 0; i < LIST_N && n < max; i++)
		if (gpu || !LIST[i].gpu) names[n++] = LIST[i].name;
	return n;
}

int p8fx_shaders(const char **names, int max)
{
	return p8fx_shaders_on(true, names, max);
}

str_id p8fx_shader_text(const char *name)
{
	int i;

	for (i = 0; name && i < LIST_N; i++)
		if (!strcmp(name, LIST[i].name)) return LIST[i].text;
	return LIST[0].text;
}

void p8fx_shader_step(bool gpu, const char *name, int d, char *out, size_t n)
{
	const char *names[LIST_N];
	int count = p8fx_shaders_on(gpu, names, LIST_N), i, at = 0;

	for (i = 0; name && i < count; i++)
		if (!strcmp(name, names[i])) at = i;
	snprintf(out, n, "%s", names[((at + (d < 0 ? -1 : 1)) % count + count) % count]);
}

bool p8fx_shrink(const void *pixels, int pitch, int tex_w, int tex_h,
                 uint8_t out[P8FX_SCREEN * P8FX_SCREEN * 4])
{
	const uint8_t *src = pixels;
	int k, x, y;

	if (!pixels || tex_w <= 0 || tex_w != tex_h || tex_w % P8FX_SCREEN || pitch < tex_w * 4) return false;
	k = tex_w / P8FX_SCREEN;
	if (k == 1 && pitch == tex_w * 4) {
		memcpy(out, src, (size_t)P8FX_SCREEN * P8FX_SCREEN * 4);
		return true;
	}
	for (y = 0; y < P8FX_SCREEN; y++) {
		const uint8_t *row = src + (size_t)y * k * (size_t)pitch;
		uint8_t *o = out + (size_t)y * P8FX_SCREEN * 4;

		for (x = 0; x < P8FX_SCREEN; x++)
			memcpy(o + x * 4, row + (size_t)x * k * 4, 4);
	}
	return true;
}

bool p8fx_covers(int x, int y, int w, int h, int out_w, int out_h)
{
	return x <= 0 && y <= 0 && (long)x + w >= out_w && (long)y + h >= out_h;
}
