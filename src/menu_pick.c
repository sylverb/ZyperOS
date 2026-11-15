/* SPDX-License-Identifier: GPL-3.0-only */
/* An option row's list (src/menu_pick.h): a panel of its own, run by the menu
 * runner like every list, over the menu it opens from. */
#include "menu_pick.h"

#include "menu.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	const menu_style *over;   /* the menu it opens from: its backdrop and power rule */
	void *over_ctx;
	bool powered;             /* closed by its power rule */
	const char *heading;
	const char *const *labels;
	int n, cur, picked;
	unsigned char sound[MENU_CHOICES_MAX];
	char word[MENU_CHOICES_MAX][64];
} pick_ctx;

int menu_pick_word(const char *label, char *out, int n)
{
	int k = snprintf(out, (size_t)n, "%s", label ? label : "");

	/* A choice in a list begins with a capital, as any list's rows do: a
	 * row's value may be "never" or "stereo" beside its name, and in a list
	 * of its own it is a name */
	if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
	return k;
}

static int pick_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	pick_ctx *p = ctx;
	int i;

	*heading = p->heading;
	for (i = 0; i < p->n && i < max && i < MENU_CHOICES_MAX; i++) {
		menu_pick_word(p->labels ? p->labels[i] : NULL, p->word[i], (int)sizeof p->word[i]);
		rows[i] = (menu_row){ p->word[i], i == p->cur ? MENU_TICK : NULL, true };
		p->sound[i] = 0;
	}
	return i;
}

static menu_result pick_key(app *a, void *ctx, in_button key, int sel)
{
	pick_ctx *p = ctx;

	(void)a;
	if (key != IN_ACCEPT || sel < 0 || sel >= p->n) return MENU_STAY;
	p->picked = sel;
	return MENU_DONE;
}

/* What is behind the menu it opens from, and what POWER means there: the
 * in-game menu's paused game and its stop, the shelf's for the rest */
static void pick_backdrop(app *a, void *ctx, float k)
{
	pick_ctx *p = ctx;

	p->over->backdrop(a, p->over_ctx, k);
}

static menu_result pick_power(app *a, void *ctx)
{
	pick_ctx *p = ctx;
	menu_result r = p->over->on_power(a, p->over_ctx);

	if (r == MENU_DONE) p->powered = true;
	return r;
}

int menu_pick(app *a, const menu_style *from, void *from_ctx, const char *heading,
              const char *const *labels, int n, int ticked, int at)
{
	static pick_ctx p;

	memset(&p, 0, sizeof p);
	p.heading = heading;
	p.labels = labels;
	p.n = n > MENU_CHOICES_MAX ? MENU_CHOICES_MAX : n;
	p.cur = ticked;
	p.picked = -1;
	p.over = from;
	p.over_ctx = from_ctx;
	int cur = at;

	menu_run(a, &(menu_style){ .accent = from ? from->accent : MENU_ZYPEROS,
	                           .follow_tint = from && from->follow_tint,
	                           .backdrop = from && from->backdrop ? pick_backdrop : NULL,
	                           .on_power = from && from->on_power ? pick_power : NULL,
	                           .fixed_w = from && from->fixed_w ? from->fixed_w : menu_std_width(a),
	                           .sound = p.sound, .start_sel = cur >= 0 && cur < p.n ? cur : 0 },
	         pick_build, pick_key, &p);
	return p.powered ? MENU_PICK_POWER : p.picked;
}

void menu_rows_print(const char *page, const menu_row *rows, int n, const unsigned char *sound,
                     const menu_choice *choices)
{
	char w[64];
	int i;

	for (i = 0; i < n; i++) {
		const menu_choice *c = menu_choice_at(choices, n, i);

		if (!rows[i].label || ROW_IS_NOTE(rows[i])) continue;
		w[0] = '\0';
		if (c && c->labels && c->cur < c->n) menu_pick_word(c->labels[c->cur], w, (int)sizeof w);
		printf("rows: %s\t%s\t%s\t%u\t%u\t%d\t%u\t%d\t%d\t%s\n", page, rows[i].label,
		       rows[i].value == MENU_TICK ? "(tick)" : rows[i].value ? rows[i].value : "",
		       (unsigned)rows[i].live, sound ? (unsigned)sound[i] : 0u, c ? c->n : 0,
		       c ? (unsigned)c->cur : 0u, c ? c->none : 0, c ? c->own : 0, w);
	}
}
