/* SPDX-License-Identifier: GPL-3.0-only */
/* Rumble's levels and what is kept for them (src/rumble.c), and the RUN key
 * a game is sent (src/config.c's cfg_run_keys_at):
 *
 *     make check-rumble
 *
 * No SDL, no device. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/config.h"
#include "../src/db.h"
#include "../src/i18n.h"
#include "../src/rumble.h"

static int fails;

static void report(int ok, const char *name)
{
	printf("  %s %s\n", ok ? "PASS" : "FAIL", name);
	if (!ok) fails++;
}

static void levels(void)
{
	char note[160];

	report(rumble_level_pct(RUMBLE_OFF) == 0 && rumble_level_pct(RUMBLE_LOW) == 35 &&
	       rumble_level_pct(RUMBLE_MEDIUM) == 65 && rumble_level_pct(RUMBLE_FULL) == 100,
	       "levels_are_0_35_65_100");
	report(RUMBLE_DEFAULT == RUMBLE_FULL, "default_is_full");
	report(rumble_level_parse("low", RUMBLE_FULL) == RUMBLE_LOW &&
	       rumble_level_parse("off", RUMBLE_FULL) == RUMBLE_OFF &&
	       rumble_level_parse("medium", RUMBLE_FULL) == RUMBLE_MEDIUM &&
	       rumble_level_parse("loud", RUMBLE_FULL) == RUMBLE_FULL &&
	       rumble_level_parse(NULL, RUMBLE_LOW) == RUMBLE_LOW, "parse_known_and_fallback");
	report(!strcmp(rumble_level_id(RUMBLE_MEDIUM), "medium") && !strcmp(rumble_level_id(RUMBLE_LEVELS), "full"),
	       "ids");
	report(!strcmp(rumble_level_name(RUMBLE_OFF), "Off") && !strcmp(rumble_level_name(RUMBLE_LOW), "Low") &&
	       !strcmp(rumble_level_name(RUMBLE_MEDIUM), "Medium") && !strcmp(rumble_level_name(RUMBLE_FULL), "Full"),
	       "names_in_english");
	rumble_level_note(RUMBLE_LOW, note, sizeof note);
	report(!strncmp(note, "35% of what a game asks.", 24), "note_low_says_35");
	rumble_level_note(RUMBLE_MEDIUM, note, sizeof note);
	report(!strncmp(note, "65% of what a game asks.", 24), "note_medium_says_65");
	report(rumble_level_step(RUMBLE_FULL, 1) == RUMBLE_FULL && rumble_level_step(RUMBLE_OFF, -1) == RUMBLE_OFF &&
	       rumble_level_step(RUMBLE_LOW, 1) == RUMBLE_MEDIUM && rumble_level_step(RUMBLE_LOW, -1) == RUMBLE_OFF,
	       "steps_stop_at_the_ends");
}

static void product(void)
{
	report(rumble_run_pct(0, RUMBLE_FULL) == 100, "full_with_no_share_is_all");
	report(rumble_run_pct(35, RUMBLE_FULL) == 35, "full_on_doom_is_doom_share");
	report(rumble_run_pct(0, RUMBLE_LOW) == 35 && rumble_run_pct(0, RUMBLE_MEDIUM) == 65, "low_medium_alone");
	report(rumble_run_pct(35, RUMBLE_LOW) == 12 && rumble_run_pct(35, RUMBLE_MEDIUM) == 23, "low_medium_on_doom");
	report(rumble_run_pct(0, RUMBLE_OFF) == 0 && rumble_run_pct(35, RUMBLE_OFF) == 0, "off_is_zero");
	report(rumble_run_pct(1, RUMBLE_LOW) == 1, "a_level_above_off_never_rounds_to_off");
}

static void run_keys(void)
{
	system_cfg s;
	char keys[128];

	memset(&s, 0, sizeof s);
	snprintf(s.tag, sizeof s.tag, "DOOM");
	s.rumble = 35;
	cfg_run_keys_at(&s, false, NULL, rumble_run_pct(s.rumble, RUMBLE_FULL), keys, (int)sizeof keys);
	report(!strcmp(keys, "rumble=35"), "doom_full_sends_its_share");
	cfg_run_keys_at(&s, false, NULL, rumble_run_pct(s.rumble, RUMBLE_OFF), keys, (int)sizeof keys);
	report(!strcmp(keys, "rumble=0"), "off_sends_rumble_0");
	s.rumble = 0;
	cfg_run_keys_at(&s, false, NULL, rumble_run_pct(s.rumble, RUMBLE_FULL), keys, (int)sizeof keys);
	report(!strcmp(keys, ""), "full_with_no_share_sends_nothing_as_before");
	cfg_run_keys_at(&s, false, NULL, rumble_run_pct(s.rumble, RUMBLE_MEDIUM), keys, (int)sizeof keys);
	report(!strcmp(keys, "rumble=65"), "medium_sends_65");
	s.rumble = 35;
	cfg_run_keys(&s, false, NULL, keys, (int)sizeof keys);
	report(!strcmp(keys, "rumble=35"), "cfg_run_keys_is_the_share_alone");
}

static void store(void)
{
	rumble_level l;
	const char *mk = "Mario Kart 64 (USA).z64";

	report(rumble_menu() == RUMBLE_FULL, "menu_full_on_a_new_card");
	report(!db_has(db_dev(), "rumble"), "and_no_key");
	rumble_menu_set(RUMBLE_LOW);
	report(rumble_menu() == RUMBLE_LOW, "menu_low_kept");
	report(rumble_scope("N64", mk) == RUMBLE_LOW && rumble_scope("N64", NULL) == RUMBLE_LOW,
	       "game_and_system_follow_menu");
	report(rumble_fallback("N64", mk) == RUMBLE_LOW && rumble_fallback("N64", NULL) == RUMBLE_LOW,
	       "both_fall_back_to_menu");
	rumble_put("N64", NULL, RUMBLE_MEDIUM);
	report(rumble_own("N64", NULL, &l) && l == RUMBLE_MEDIUM, "system_level_kept");
	report(rumble_scope("N64", mk) == RUMBLE_MEDIUM && rumble_fallback("N64", mk) == RUMBLE_MEDIUM,
	       "game_follows_its_system");
	rumble_put("N64", mk, RUMBLE_OFF);
	report(rumble_own("N64", mk, &l) && l == RUMBLE_OFF && rumble_scope("N64", mk) == RUMBLE_OFF,
	       "game_level_kept");
	report(db_has(db_lib(), "rumble.N64.Mario Kart 64 (USA).z64"), "under_its_whole_name");
	rumble_put("N64", mk, RUMBLE_MEDIUM);
	report(!rumble_own("N64", mk, &l) && !db_has(db_lib(), "rumble.N64.Mario Kart 64 (USA).z64"),
	       "choosing_the_fallback_removes_the_key");
	rumble_put("N64", NULL, RUMBLE_LOW);
	report(!rumble_own("N64", NULL, &l), "system_back_to_menu_removes_its_key");
	db_set_str(db_lib(), "rumble.N64", "loud");
	report(!rumble_own("N64", NULL, &l) && rumble_scope("N64", NULL) == RUMBLE_LOW,
	       "a_word_it_cannot_read_falls_back");
	db_del(db_lib(), "rumble.N64");
	rumble_menu_set(RUMBLE_FULL);
	report(!db_has(db_dev(), "rumble"), "menu_back_to_full_removes_the_key");
}

int main(void)
{
	char dev[] = "/tmp/rumble-check-dev-XXXXXX", lib[] = "/tmp/rumble-check-lib-XXXXXX";
	int fd = mkstemp(dev), fl = mkstemp(lib);

	printf("rumble: the levels\n");
	levels();
	product();
	run_keys();
	printf("rumble: kept on the card\n");
	if (fd >= 0) close(fd);
	if (fl >= 0) close(fl);
	if (fd < 0 || fl < 0 || !db_init(dev, lib, "/dev/null")) {
		printf("  (no temporary database)\n");
		fails++;
	} else {
		store();
		db_shutdown();
	}
	unlink(dev);
	unlink(lib);
	if (fails) {
		printf("\n%d failure%s\n", fails, fails == 1 ? "" : "s");
		return 1;
	}
	printf("ok\n");
	return 0;
}
