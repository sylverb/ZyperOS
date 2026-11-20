/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_NIGHTKEEP_H
#define ZYPEROS_NIGHTKEEP_H
/* Night Mode's settings in the device's database (src/nightmode.h), and the
 * first start's migration from 1.0.5's Appearance Auto hours:
 *
 *   night.mode      "off", "on" or "hours"
 *   night.warmth    1 to 10
 *   night.from      minutes into the local day
 *   night.until     minutes into the local day
 *   night.auto_106  "1" once the migration ran (NIGHT_MIGRATED_KEY)
 *
 * The Night Hours are Appearance Auto's hours too (src/theme.h): the theme
 * is told them at the start and at every change kept here.
 *
 * The launcher's: the database is its. */
#include "nightmode.h"

/* The migration's mark, as timezone.auto_105 and coreopts.runtime_106 are
 * theirs: 1.0.6's move of Auto's hours to the Night Hours is done on this
 * card. 1.0.6 itself kept no mark, and needed none: it turned every Auto
 * into Light or Dark, so a second start found nothing. Since 1.1.0 Auto is
 * a setting again, and only the mark tells a 1.0.5 card's Auto from one
 * picked since. */
#define NIGHT_MIGRATED_KEY "night.auto_106"

/* The kept settings, put right (night_cfg_clamp); the defaults for any key
 * not there */
night_cfg night_load(void);

/* Appearance Auto's hours (theme_set_hours) from n's Night Hours, put right:
 * light from until, dark from from. At the start, before theme_init, and by
 * night_keep with every change. The theme is not looked at again here: the
 * caller that changed the hours asks theme_refresh. */
void night_theme_hours(const night_cfg *n);

/* n put right and kept, and on the display at once: the table now and the
 * file Diatom and portkeys read (plat_night); and its hours Auto's
 * (night_theme_hours) */
void night_keep(const night_cfg *n);

/* The migration (night_migrate) on the database: before the appearance and
 * the Night Hours are read at the start. Once a card: it keeps
 * NIGHT_MIGRATED_KEY, and with it there does nothing, so a second start, and
 * Auto picked since, are left alone. Says what it did on stderr. */
void night_migrate_db(void);

#endif
