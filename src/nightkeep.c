/* SPDX-License-Identifier: GPL-3.0-only */
/* Night Mode's settings in the database. See src/nightkeep.h. */
#include "nightkeep.h"

#include <stdio.h>

#include "db.h"

/* platform.h's, without its SDL: the kept settings on the display */
void plat_night(const struct night_cfg *n);

night_cfg night_load(void)
{
	night_cfg n = night_default();
	char w[16];

	db_get_str(db_dev(), "night.mode", w, sizeof w, night_mode_id(NIGHT_OFF));
	n.mode = night_mode_parse(w);
	n.warmth = db_get_int(db_dev(), "night.warmth", NIGHT_WARMTH_DEFAULT);
	n.from = db_get_int(db_dev(), "night.from", NIGHT_FROM_DEFAULT);
	n.until = db_get_int(db_dev(), "night.until", NIGHT_UNTIL_DEFAULT);
	night_cfg_clamp(&n);
	return n;
}

void night_keep(const night_cfg *in)
{
	night_cfg n = *in;

	night_cfg_clamp(&n);
	db_set_str(db_dev(), "night.mode", night_mode_id(n.mode));
	db_set_int(db_dev(), "night.warmth", n.warmth);
	db_set_int(db_dev(), "night.from", n.from);
	db_set_int(db_dev(), "night.until", n.until);
	plat_night(&n);
}

void night_migrate_db(time_t now)
{
	db *d = db_dev();
	char ap[16];
	bool have_ap = db_has(d, "appearance");
	bool have_light = db_has(d, "auto.light"), have_dark = db_has(d, "auto.dark");
	night_migration m;

	if (!d) return;
	db_get_str(d, "appearance", ap, sizeof ap, "");
	m = night_migrate(have_ap ? ap : NULL, have_light, db_get_int(d, "auto.light", -1),
	                  have_dark, db_get_int(d, "auto.dark", -1),
	                  db_has(d, "night.from") || db_has(d, "night.until"), now);
	if (m.appearance) {
		db_set_str(d, "appearance", m.appearance);
		fprintf(stderr, "night: Appearance Auto is gone; %s, the theme it gave now\n",
		        m.appearance);
	}
	if (m.hours) {
		db_set_int(d, "night.from", m.from);
		db_set_int(d, "night.until", m.until);
		fprintf(stderr, "night: Auto's hours kept as Night Hours, %02d:%02d to %02d:%02d\n",
		        m.from / 60, m.from % 60, m.until / 60, m.until % 60);
	}
	if (m.drop_auto) {
		db_del(d, "auto.light");
		db_del(d, "auto.dark");
	}
}
