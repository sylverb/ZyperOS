/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_COREOPTS_H
#define ZYPEROS_COREOPTS_H

/* The core options a game is sent (src/platform.c, plat_coreopt): the
 * player's own rows in the library database ("coreopt.<tag>.<option>", an
 * empty tag for every system), and under them the defaults, given at run
 * time and never written to the library: the device's (its profile's
 * coreopts, src/device/profile.h), then every device's (here). So a row in
 * the library is only ever the player's, and it holds on whatever handheld
 * the card is in. No SDL. */

#include <stdbool.h>

#include "db.h"
#include "device/profile.h"   /* dev_default */

/* One option as it is sent: its system's tag ("" for every system) and
 * "option=value" */
typedef struct { char tag[8]; char kv[192]; } coreopt;

/* Adds `key` ("coreopt.<tag>.<option>") = `value` to `set` (`*n` of `max`)
 * unless it holds that tag's option already. False, and nothing added,
 * for a key with no tag, a tag of 8 characters or more, a pair too long for
 * `kv` (half a pair would still look meant), or a full set. */
bool coreopt_add(coreopt *set, int *n, int max, const char *key, const char *value);

/* The defaults under what `set` holds: the device's `dev` (ending in
 * { NULL, NULL }; NULL for none), then every device's, each only where
 * `set` has none of that option; then `set` sorted by key as the library's
 * own scan is, so the order sent is the order 1.0.5 sent. A default that
 * does not fit is dropped, and said on stderr. */
void coreopt_defaults(coreopt *set, int *n, int max, const dev_default *dev);

/* The `want`th option `tag`'s games are sent, from 0, into *out: every
 * system's first, then the tag's own, so a system's overrides one for
 * every system. 0 past the last. */
int coreopt_nth(const coreopt *set, int n, const char *tag, int want, const char **out);

/* Every device's defaults, ending in { NULL, NULL } */
const dev_default *coreopt_shared(void);

/* Once on a card (COREOPTS_MIGRATED_KEY says it was): the rows 1.0.5 seeded
 * and nobody changed - each "coreopt." row whose value is every device's
 * default or the Pixel 2's for its key - taken out of the library, so a
 * card moved to another handheld does not carry the Pixel 2's tuning. A
 * row the player changed stays. True on the start that did it. */
#define COREOPTS_MIGRATED_KEY "coreopts.runtime_106"
bool coreopts_migrate(db *lib);

#endif
