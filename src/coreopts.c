/* SPDX-License-Identifier: GPL-3.0-only */
/* The core options' defaults at run time. See src/coreopts.h. */
#include "coreopts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Every device's: what a core needs whatever it runs on. The keys and
 * values each core's own (its libretro_core_options.h). The segment after
 * "coreopt." is the system tag and an EMPTY one means every system, which
 * is why most keys have two dots. A tagged entry overrides a global of the
 * same name. */
static const dev_default SHARED[] = {
	{ "coreopt..mgba_sgb_borders",  "OFF" },
	{ "coreopt.GB.mgba_gb_model",   "Game Boy" },
	{ "coreopt.GB.mgba_gb_colors",  "DMG Green" },
	/* Gambatte (1.0.7), the Game Boy shelves' second core: the Game Boy's
	 * games as mGBA shows them, the first Game Boy's green on a Game Boy
	 * (its "Internal" palette, GB - DMG), where the core's own default is
	 * no color at all; the Game Boy shelf a Game Boy, as mGBA's model is.
	 * Read from libgambatte/libretro/libretro_core_options.h at the commit
	 * mk/build-small-cores.sh builds. */
	{ "coreopt.GB.gambatte_gb_colorization",    "internal" },
	{ "coreopt.GB.gambatte_gb_internal_palette", "GB - DMG" },
	{ "coreopt.GB.gambatte_gb_hwmode",           "GB" },
	/* pcsx_rearmed's frameskip on "auto": Diatom drives it with a hint
	 * (diatom/src/env.c), and its "auto" skips a frame reported as about to
	 * underrun, which Diatom says only of frames fast forward will not show.
	 * The PlayStation's fast forward is not yet measured on the Pixel 2. */
	{ "coreopt..pcsx_rearmed_frameskip_type", "auto" },
	/* PicoDrive's, for the Sega CD and 32X, the same way: not yet measured
	 * on the Pixel 2. */
	{ "coreopt..picodrive_frameskip", "auto" },
	/* And the three cores whose heavy games fell short of fast forward's
	 * speed on the Pixel 2, every frame drawn though most are never shown:
	 * mGBA (the Game Boys and the GBA), Snes9x 2010 and Genesis Plus GX (the
	 * Genesis, Master System and Game Gear). Each core's own name for the
	 * option and for its automatic value, "auto", which skips a frame only
	 * when the frontend says it is about to underrun - in Diatom, only a
	 * frame fast forward will not show, so at 1x nothing changes. Read from
	 * each core's libretro_core_options.h and the strcmp in its libretro.c
	 * (libretro/mgba at e31759b, the tree mk/build-mgba-bridge.sh builds;
	 * libretro/snes9x2010 and libretro/Genesis-Plus-GX), where each one's
	 * own default is "disabled". */
	{ "coreopt..mgba_frameskip",            "auto" },
	{ "coreopt..snes9x_2010_frameskip",     "auto" },
	{ "coreopt..genesis_plus_gx_frameskip", "auto" },
	/* PicoDrive's own default, PAR, reports square pixels, 1.4286 for
	 * 320x224, where genesis_plus_gx reports 1.3061 for the same Genesis
	 * picture. CRT reports 1.2991, so the Sega CD and 32X shelves take the
	 * Genesis shelf's shape. Measured in Diatom's desktop port, 2026-11-03. */
	{ "coreopt..picodrive_aspect",  "CRT" },
	/* flycast2021, the Dreamcast's core (config/systems.cfg). The fork starts on
	 * its own HLE BIOS, which left Crazy Taxi 2 on the license screen; the
	 * real one is in Bios/dc. */
	{ "coreopt..reicast_hle_bios", "disabled" },
	/* ScummVM's buttons. The core's own defaults put the mouse's two buttons
	 * on L and R and the keyboard on the faces (A space, B return), where a
	 * point-and-click game is played with the pointer: the A cap clicks, the
	 * B cap right-clicks, and space and return move to X and L1. Y stays
	 * escape (skips a cutscene), START the ScummVM menu (save and load),
	 * SELECT the virtual keyboard, R2 the slow pointer, and R1 joins it. */
	{ "coreopt.SCUMMVM.scummvm_mapper_a", "RETROKE_LEFT_BUTTON" },
	{ "coreopt.SCUMMVM.scummvm_mapper_b", "RETROKE_RIGHT_BUTTON" },
	{ "coreopt.SCUMMVM.scummvm_mapper_x", "RETROK_SPACE" },
	{ "coreopt.SCUMMVM.scummvm_mapper_l", "RETROK_RETURN" },
	{ "coreopt.SCUMMVM.scummvm_mapper_r", "RETROKE_FINE_CONTROL" },
	/* MAME 2003-Plus, straight to the game: past the disclaimer and the
	 * warning screens, which a press would dismiss on every start. */
	{ "coreopt..mame2003-plus_skip_disclaimer", "enabled" },
	{ "coreopt..mame2003-plus_skip_warnings",   "enabled" },
	/* The CDZ's BIOS, where the core's default is the front loader's: with
	 * front-sp1.bin, Metal Slug sat in the CD player screen with PUSH START
	 * BUTTON after 40 seconds; with neocd.bin it was at its own menu. The
	 * value is the core's own label for the file, which is why the Neo Geo
	 * CD shelf's disc bios is neocd/neocd.bin (config/systems.cfg). */
	{ "coreopt..neocd_bios",        "neocd.bin (CDZ)" },
	{ NULL, NULL },
};

#define PREFIX "coreopt."

const dev_default *coreopt_shared(void)
{
	return SHARED;
}

/* The option's name in `kv`, up to its '=', as a length */
static size_t opt_len(const char *kv)
{
	return strcspn(kv, "=");
}

/* Whether `set` has `tag`'s option `opt` (`len` characters) */
static bool has(const coreopt *set, int n, const char *tag, const char *opt, size_t len)
{
	int i;

	for (i = 0; i < n; i++)
		if (!strcmp(set[i].tag, tag) && opt_len(set[i].kv) == len && !strncmp(set[i].kv, opt, len))
			return true;
	return false;
}

bool coreopt_add(coreopt *set, int *n, int max, const char *key, const char *value)
{
	const char *rest, *dot;
	char tag[sizeof set[0].tag];
	size_t tl;

	if (strncmp(key, PREFIX, strlen(PREFIX))) return false;
	rest = key + strlen(PREFIX);
	dot = strchr(rest, '.');
	if (!dot || !dot[1]) return false;
	tl = (size_t)(dot - rest);
	if (tl >= sizeof tag) return false;
	memcpy(tag, rest, tl);
	tag[tl] = '\0';
	if (has(set, *n, tag, dot + 1, strlen(dot + 1))) return false;
	if (*n >= max) return false;
	if (snprintf(set[*n].kv, sizeof set[0].kv, "%s=%s", dot + 1, value) >= (int)sizeof set[0].kv)
		return false;
	memcpy(set[*n].tag, tag, tl + 1);
	(*n)++;
	return true;
}

/* As the library's ORDER BY key: "coreopt.<tag>.<option>", byte by byte,
 * so every system's (the empty tag, '.' first) come before a tag's */
static int by_key(const void *pa, const void *pb)
{
	const coreopt *a = pa, *b = pb;
	char ka[sizeof a->tag + sizeof a->kv + 2], kb[sizeof ka];

	snprintf(ka, sizeof ka, "%s.%.*s", a->tag, (int)opt_len(a->kv), a->kv);
	snprintf(kb, sizeof kb, "%s.%.*s", b->tag, (int)opt_len(b->kv), b->kv);
	return strcmp(ka, kb);
}

/* One default under `set`: false when it was not there and did not fit */
static bool under(coreopt *set, int *n, int max, const dev_default *d)
{
	const char *rest = d->key + strlen(PREFIX), *dot = strchr(rest, '.');
	char tag[sizeof set[0].tag];
	size_t tl;

	if (!dot || (tl = (size_t)(dot - rest)) >= sizeof tag) return true;
	memcpy(tag, rest, tl);
	tag[tl] = '\0';
	return has(set, *n, tag, dot + 1, strlen(dot + 1)) || coreopt_add(set, n, max, d->key, d->value);
}

void coreopt_defaults(coreopt *set, int *n, int max, const dev_default *dev)
{
	const dev_default *d;
	int lost = 0;

	for (d = dev; d && d->key; d++) lost += !under(set, n, max, d);
	for (d = SHARED; d->key; d++) lost += !under(set, n, max, d);
	if (lost) fprintf(stderr, "coreopts: no room for %d of the defaults\n", lost);
	qsort(set, (size_t)*n, sizeof *set, by_key);
}

int coreopt_nth(const coreopt *set, int n, const char *tag, int want, const char **out)
{
	int pass, i, seen = 0;

	for (pass = 0; pass < 2; pass++)
		for (i = 0; i < n; i++) {
			const bool global = set[i].tag[0] == '\0';

			if (pass == 0 ? !global : global) continue;
			if (pass == 1 && (!tag || strcmp(set[i].tag, tag))) continue;
			if (seen++ == want) {
				if (out) *out = set[i].kv;
				return 1;
			}
		}
	return 0;
}

/* `t`'s rows taken out of `lib` where they still hold the value `t` gives */
static void take_out(db *lib, const dev_default *t)
{
	char v[192];

	for (; t->key; t++)
		if (db_get_str(lib, t->key, v, sizeof v, NULL) && !strcmp(v, t->value))
			db_del(lib, t->key);
}

bool coreopts_migrate(db *lib)
{
	if (!lib || db_get_int(lib, COREOPTS_MIGRATED_KEY, 0) == 1) return false;
	take_out(lib, SHARED);
	take_out(lib, dev_coreopts_pixel2);
	db_set_str(lib, COREOPTS_MIGRATED_KEY, "1");
	return true;
}
