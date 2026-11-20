/* SPDX-License-Identifier: GPL-3.0-only */
/* MENU > Display > Home Systems (src/homesys.h, DESIGN.md 11.48): a switch
 * for Favorites, Recents, Collections and Screenshots (each once the card
 * has one) and every system the card has games for, Left,
 * Right or A turning one over, as Side Lights' are. The home is built again
 * once, as the page closes on a change, from the saved library
 * (shelves_reload): a system turned on comes back without a read of the
 * card, and the player is where they were, or on the home when the shelf
 * they stood on went. */
#include "homesys_screen.h"

#include "collect.h"
#include "db.h"
#include "homesys.h"
#include "menu_panel.h"
#include "panel.h"
#include "shelfbuild.h"
#include "sys_menu.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

typedef struct {
	app *a;
	const char *names[CFG_MAX_SYSTEMS], *tags[CFG_MAX_SYSTEMS];
	int n;
	bool changed;
	unsigned char sound[HS_ROWS];
} homesys_page;

static int hs_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	homesys_page *pg = ctx;
	homesys_ui u = { pg->names, pg->tags, pg->n, coll_count() > 0, shelf_shots_have() };
	const char *ids[HS_ROWS];
	int n, i;

	(void)max;
	*heading = T(S_SYSMENU_HOME_SYSTEMS);
	n = sys_menu_homesys_rows(&u, rows, ids);
	for (i = 0; i < n; i++) pg->sound[i] = ids[i] && rows[i].live ? MENU_SND_RING : 0;
	return n;
}

static menu_result hs_key(app *a, void *ctx, in_button key, int sel)
{
	homesys_page *pg = ctx;
	homesys_ui u = { pg->names, pg->tags, pg->n, coll_count() > 0, shelf_shots_have() };
	menu_row rows[HS_ROWS];
	const char *ids[HS_ROWS];
	int n = sys_menu_homesys_rows(&u, rows, ids);
	char word[HOMESYS_WORD];

	(void)a;
	if (key != IN_LEFT && key != IN_RIGHT && key != IN_ACCEPT) return MENU_STAY;
	if (sel < 0 || sel >= n || !ids[sel] || !rows[sel].live) return MENU_STAY;
	homesys_set(ids[sel], !homesys_shown(ids[sel]));
	homesys_word(word, sizeof word);
	db_set_str(db_dev(), HOMESYS_KEY, word);
	fprintf(stderr, "home: %s %s (off: %s)\n", ids[sel], homesys_shown(ids[sel]) ? "on" : "off",
	        word[0] ? word : "none");
	pg->changed = true;
	return MENU_STAY;
}

void homesys_screen(app *a)
{
	static homesys_page pg;
	const shelf_have *have;
	int i;

	memset(&pg, 0, sizeof pg);
	pg.a = a;
	pg.n = shelf_have_list(&have);
	for (i = 0; i < pg.n; i++) {
		pg.names[i] = have[i].name;
		pg.tags[i] = have[i].tag;
	}
	menu_run(a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(a),
	                           .sound = pg.sound },
	         hs_build, hs_key, &pg);
	if (!pg.changed) return;
	wait_panel(a, T(S_SYSMENU_HOME_SYSTEMS), T(S_LIBRARY_SCANNING));
	shelves_reload(a);
}

void homesys_load(void)
{
	char word[HOMESYS_WORD];

	db_get_str(db_dev(), HOMESYS_KEY, word, sizeof word, "");
	homesys_parse(word);
}

const char *homesys_value(char *out, size_t n)
{
	const shelf_have *have;
	/* Collections is one of them once the card has a collection, and
	 * Screenshots once it has a shot */
	bool shots = shelf_shots_have();
	int groups = HS_GROUPS - (coll_count() > 0 ? 0 : 1) - (shots ? 0 : 1);
	int total = shelf_have_list(&have) + groups, on = groups, i;

	on -= !homesys_shown(HOMESYS_FAV) + !homesys_shown(HOMESYS_RECENT) +
	      (coll_count() > 0 && !homesys_shown(HOMESYS_COLL)) + (shots && !homesys_shown(HOMESYS_SHOT));
	for (i = 0; i < total - groups; i++) on += homesys_shown(have[i].tag);
	if (on >= total) return T(S_SYSMENU_HOME_SYSTEMS_ALL);
	snprintf(out, n, T(S_SYSMENU_HOME_SYSTEMS_SOME), on, total);
	return out;
}

void homesys_shot_draw(app *a, int row)
{
	static homesys_page pg;
	menu_row rows[HS_ROWS];
	const char *heading;
	const shelf_have *have;
	int n, i;

	memset(&pg, 0, sizeof pg);
	pg.n = shelf_have_list(&have);
	for (i = 0; i < pg.n; i++) {
		pg.names[i] = have[i].name;
		pg.tags[i] = have[i].tag;
	}
	n = hs_build(&pg, rows, HS_ROWS, &heading);
	ui_scrim(a->r, 0);
	menu_draw(a, heading, rows, n, row >= 0 && row < n ? row : 0, menu_std_width(a), MENU_ZYPEROS);
}

