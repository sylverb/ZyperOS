/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_NIGHTKEEP_H
#define ZYPEROS_NIGHTKEEP_H
/* Night Mode's settings in the device's database (src/nightmode.h), and the
 * first start's migration from Appearance's Auto:
 *
 *   night.mode    "off", "on" or "hours"
 *   night.warmth  1 to 10
 *   night.from    minutes into the local day
 *   night.until   minutes into the local day
 *
 * The launcher's: the database is its. */
#include "nightmode.h"

/* The kept settings, put right (night_cfg_clamp); the defaults for any key
 * not there */
night_cfg night_load(void);

/* n put right and kept, and on the display at once: the table now and the
 * file Diatom and portkeys read (plat_night) */
void night_keep(const night_cfg *n);

/* The migration (night_migrate) on the database, at `now`: before the
 * appearance is read at the start, so the theme it reads is the one the
 * migration left. Once: it leaves nothing for a second start to do. Says
 * what it did on stderr. */
void night_migrate_db(time_t now);

#endif
