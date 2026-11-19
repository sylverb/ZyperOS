/* SPDX-License-Identifier: GPL-3.0-only */
#include "rumble.h"

#include <stdio.h>
#include <string.h>

#include "db.h"
#include "i18n.h"
#include "library.h"

/* As long as a game's whole key, so a long name is never cut: Rename and
 * Delete Game build the same key whole (src/gamern.c, src/gamedel.c) */
#define KEY_MAX (64 + LIB_PATH + 8)

#define MENU_KEY "rumble"

static const char *const IDS[RUMBLE_LEVELS] = { "off", "low", "medium", "full" };
/* Low and Medium: my pick in the Sound lab (2026-11-19), of 25/50, 35/65
 * and 50/75. Low is Doom's own share, a third, felt on the Pixel 2's motor. */
static const int PCT[RUMBLE_LEVELS] = { 0, 35, 65, 100 };

static rumble_level valid(rumble_level l)
{
	return l >= RUMBLE_OFF && l < RUMBLE_LEVELS ? l : RUMBLE_DEFAULT;
}

int rumble_level_pct(rumble_level l) { return PCT[valid(l)]; }

const char *rumble_level_id(rumble_level l) { return IDS[valid(l)]; }

rumble_level rumble_level_parse(const char *id, rumble_level fallback)
{
	int i;

	if (id)
		for (i = 0; i < RUMBLE_LEVELS; i++)
			if (!strcmp(id, IDS[i])) return (rumble_level)i;
	return valid(fallback);
}

const char *rumble_level_name(rumble_level l)
{
	switch (valid(l)) {
	case RUMBLE_OFF:    return T(S_MENU_OFF_TITLE);
	case RUMBLE_LOW:    return T(S_RUMBLE_LOW);
	case RUMBLE_MEDIUM: return T(S_RUMBLE_MEDIUM);
	default:            return T(S_RUMBLE_FULL);
	}
}

void rumble_level_note(rumble_level l, char *out, size_t n)
{
	if (!n) return;
	switch (valid(l)) {
	case RUMBLE_OFF:  snprintf(out, n, "%s", T(S_RUMBLE_NOTE_OFF)); break;
	case RUMBLE_FULL: snprintf(out, n, "%s", T(S_RUMBLE_NOTE_FULL)); break;
	default:          snprintf(out, n, T(S_RUMBLE_NOTE_SHARE), PCT[valid(l)]); break;
	}
}

rumble_level rumble_level_step(rumble_level l, int dir)
{
	int k = (int)valid(l) + (dir < 0 ? -1 : dir > 0 ? 1 : 0);

	if (k < RUMBLE_OFF) k = RUMBLE_OFF;
	if (k > RUMBLE_FULL) k = RUMBLE_FULL;
	return (rumble_level)k;
}

int rumble_run_pct(int share, rumble_level l)
{
	const int lv = PCT[valid(l)];
	int pct;

	if (share < 1 || share > 99) share = 100;
	if (lv <= 0) return 0;
	pct = (share * lv + 50) / 100;
	/* Low on Doom is 12%: never 0, which is Off */
	return pct < 1 ? 1 : pct;
}

rumble_level rumble_menu(void)
{
	char v[16];

	if (!db_dev() || !db_get_str(db_dev(), MENU_KEY, v, sizeof v, NULL)) return RUMBLE_DEFAULT;
	return rumble_level_parse(v, RUMBLE_DEFAULT);
}

void rumble_menu_set(rumble_level l)
{
	if (!db_dev()) return;
	/* Full is a new card's: no key, so a card that never chose has none */
	if (valid(l) == RUMBLE_DEFAULT) db_del(db_dev(), MENU_KEY);
	else                            db_set_str(db_dev(), MENU_KEY, IDS[valid(l)]);
}

static void key_of(const char *tag, const char *file, char *out, size_t n)
{
	if (file && file[0]) snprintf(out, n, "rumble.%s.%s", tag, file);
	else                 snprintf(out, n, "rumble.%s", tag);
}

bool rumble_own(const char *tag, const char *file, rumble_level *out)
{
	char key[KEY_MAX], v[16];

	if (!tag || !tag[0] || !db_lib()) return false;
	key_of(tag, file, key, sizeof key);
	if (!db_get_str(db_lib(), key, v, sizeof v, NULL)) return false;
	/* A word a newer release wrote is no level here: the fallback */
	for (int i = 0; i < RUMBLE_LEVELS; i++)
		if (!strcmp(v, IDS[i])) { *out = (rumble_level)i; return true; }
	return false;
}

rumble_level rumble_fallback(const char *tag, const char *file)
{
	rumble_level l;

	if (file && file[0] && rumble_own(tag, NULL, &l)) return l;
	return rumble_menu();
}

rumble_level rumble_scope(const char *tag, const char *file)
{
	rumble_level l;

	return rumble_own(tag, file, &l) ? l : rumble_fallback(tag, file);
}

void rumble_put(const char *tag, const char *file, rumble_level l)
{
	char key[KEY_MAX];

	if (!tag || !tag[0] || !db_lib()) return;
	key_of(tag, file, key, sizeof key);
	if (valid(l) == rumble_fallback(tag, file)) db_del(db_lib(), key);
	else                                        db_set_str(db_lib(), key, IDS[valid(l)]);
}
