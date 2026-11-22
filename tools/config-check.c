/* systems.cfg's accent field: a color, or no color at all.
 *
 * A system's line names its color as six hex digits. One that names none -
 * the field empty, left off the end of the line, or holding something that is
 * not a color - used to come out cyan, which DESIGN.md 3.3 takes out of the
 * system, and a typo came out black. Now it is CFG_NO_COLOR, which no line
 * can name, and the screens draw what nothing looks like by role: text-dim
 * for a mark (src/ui.h, ui_sys_mark). This holds the reading side of that:
 * which fields are a color and which are not, and that "not" can never be
 * mistaken for a color a line could have named.
 *
 * And the lines that add to a system by its tag - power, map, controller -
 * and the RUN keys made from them (cfg_run_keys).
 *
 * Links src/config.c alone, and NOT SDL. Writes its one file under a scratch
 * directory of its own.
 */
#include "../src/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

/* The system named `name` in what was read, or NULL. */
static const system_cfg *find(const systems_cfg *c, const char *name)
{
	int i;

	for (i = 0; i < c->count; i++)
		if (!strcmp(c->systems[i].name, name)) return &c->systems[i];
	return NULL;
}

/* The shipped pace lines (src/pacing.h): Timing, Smooth by default, on the
 * Game Boys, the DS and the Genesis, and on no other shelf */
static void pace_lines_check(void)
{
	static systems_cfg c;
	static const char *const smooth[] = { "GB", "GBC", "GBA", "NDS", "MD" };
	int i, k, others = 0, found = 0;

	ck(cfg_load_systems("config/systems.cfg", &c), "config/systems.cfg loads");
	for (i = 0; i < c.count; i++) {
		bool listed = false;

		for (k = 0; k < (int)(sizeof smooth / sizeof smooth[0]); k++)
			if (!strcmp(c.systems[i].tag, smooth[k])) listed = true;
		if (listed && !strcmp(c.systems[i].pace, "smooth")) found++;
		else if (!listed && c.systems[i].pace[0]) { printf("    %s has Timing\n", c.systems[i].tag); others++; }
	}
	ck(found == (int)(sizeof smooth / sizeof smooth[0]), "Timing, Smooth, on the GB, GBC, GBA, DS and Genesis shelves");
	ck(!others, "and on no other shelf: the rest keep the core's own rate, no row");
}

/* The shipped es lines: each name once in the file, none that the card's
 * file system would take as a shelf's own folder (exFAT ignores case),
 * every line's tag a system's, and every line whole: the field holds 159
 * characters, and a longer line would lose its last names */
static void es_names_check(void)
{
	static systems_cfg c;
	char names[512][CFG_STR], one[CFG_STR];
	int n = 0, i, k, j, lines = 0, tags = 0, dup = 0, own = 0, cut = 0;
	FILE *f;
	char line[1024];

	ck(cfg_load_systems("config/systems.cfg", &c), "config/systems.cfg loads");
	for (i = 0; i < c.count; i++) {
		if (c.systems[i].es[0]) tags++;
		for (k = 0; cfg_es_at(&c.systems[i], k, one, sizeof one) && n < 512; k++) {
			for (j = 0; j < n; j++)
				if (!strcasecmp(names[j], one)) { printf("    %s twice\n", one); dup++; }
			for (j = 0; j < c.count; j++)
				if (!strcasecmp(c.systems[j].folder, one)) {
					printf("    %s is %s's own folder\n", one, c.systems[j].folder);
					own++;
				}
			snprintf(names[n++], sizeof names[0], "%s", one);
		}
	}
	ck(n > 60, "the shipped shelves have their ES names");
	ck(!dup, "an ES name belongs to one shelf");
	ck(!own, "no ES name is a shelf's own folder in other capitals");
	if (!(f = fopen("config/systems.cfg", "r"))) { ck(0, "systems.cfg opens"); return; }
	while (fgets(line, sizeof line, f)) {
		char *tag = line + 3, *list;

		if (strncmp(line, "es|", 3)) continue;
		lines++;
		if (!(list = strchr(tag, '|'))) continue;
		*list++ = '\0';
		list[strcspn(list, "\r\n")] = '\0';
		for (i = 0; i < c.count; i++)
			if (!strcmp(c.systems[i].tag, tag) && strcmp(c.systems[i].es, list)) {
				printf("    es|%s is cut to %s\n", tag, c.systems[i].es);
				cut++;
			}
	}
	fclose(f);
	ck(lines == tags, "every es line's tag is a system's");
	ck(!cut, "and every es line read whole");
}

/* The folders other systems keep a console's games in, each read on its
 * shelf alone: ZyperOS's own folder in other capitals, or one of that
 * shelf's es names. "playstation" first, which a player coming from
 * ROCKNIX asked about (2026-11-20): it is Roms/PlayStation on the card,
 * exFAT ignoring case. Then every folder under ROCKNIX's roms/ for a
 * system ZyperOS has (its config/emulators, SYSTEM_PATH, read that day),
 * and the folders Knulli and Batocera, ArkOS, RetroPie and Recalbox use
 * for those systems where ROCKNIX's differ. AmberELEC's and JELOS's are
 * all ROCKNIX's. */
static void es_folders_check(void)
{
	static const struct { const char *folder, *tag; } TO[] = {
		{ "playstation", "PS1" },
		/* ROCKNIX */
		{ "3do", "3DO" }, { "arcade", "ARCADE" }, { "atari2600", "A2600" },
		{ "atari5200", "A5200" }, { "atari7800", "A7800" }, { "atarilynx", "LYNX" },
		{ "atomiswave", "AW" }, { "coleco", "COLECO" }, { "cps1", "ARCADE" },
		{ "cps2", "ARCADE" }, { "cps3", "ARCADE" }, { "doom", "DOOM" },
		{ "dreamcast", "DC" }, { "famicom", "NES" }, { "fbneo", "ARCADE" },
		{ "fds", "NES" }, { "gameandwatch", "GW" }, { "gamegear", "GG" },
		{ "gamegearh", "GG" }, { "gb", "GB" }, { "gba", "GBA" }, { "gbah", "GBA" },
		{ "gbav", "GBA" }, { "gbc", "GBC" }, { "gbch", "GBC" }, { "gbh", "GB" },
		{ "genesis", "MD" }, { "genh", "MD" }, { "j2me", "J2ME" }, { "mame", "MAME" },
		{ "mastersystem", "SMS" }, { "megacd", "SEGACD" }, { "megadrive", "MD" },
		{ "megadrive-japan", "MD" }, { "megadriveh", "MD" }, { "msx", "MSX2" },
		{ "msx2", "MSX2" }, { "n64", "N64" }, { "naomi", "NAOMI" }, { "nds", "NDS" },
		{ "neocd", "NEOCD" }, { "neogeo", "NEOGEO" }, { "nes", "NES" },
		{ "nesh", "NES" }, { "ngp", "NGP" }, { "ngpc", "NGPC" },
		{ "openbor", "OPENBOR" }, { "pcengine", "PCE" }, { "pcenginecd", "PCE" },
		{ "pico-8", "PICO8" }, { "pokemini", "PKMINI" }, { "ports", "PRT" },
		{ "psp", "PSP" }, { "pspminis", "PSP" }, { "psx", "PS1" },
		{ "saturn", "SATURN" }, { "sega32x", "32X" }, { "segacd", "SEGACD" },
		{ "sfc", "SFC" }, { "sg-1000", "SG1000" }, { "sgfx", "PCE" },
		{ "snes", "SFC" }, { "snesh", "SFC" }, { "tg16", "PCE" },
		{ "tg16cd", "PCE" }, { "tic-80", "TIC80" }, { "vectrex", "VECTREX" },
		{ "virtualboy", "VB" }, { "wonderswan", "WS" },
		{ "wonderswancolor", "WSC" },
		/* Knulli and Batocera, ArkOS, RetroPie, Recalbox */
		{ "colecovision", "COLECO" }, { "fba", "ARCADE" }, { "gw", "GW" },
		{ "lynx", "LYNX" }, { "mame-libretro", "MAME" }, { "mame2003", "MAME" },
		{ "msx1", "MSX2" }, { "msx2+", "MSX2" }, { "neogeocd", "NEOCD" },
		{ "pico8", "PICO8" }, { "prboom", "DOOM" }, { "scummvm", "SCUMMVM" },
		{ "sg1000", "SG1000" }, { "sgb", "GB" }, { "snes-hacks", "SFC" },
		{ "supergrafx", "PCE" }, { "tic80", "TIC80" }, { "turbografx", "PCE" },
		{ "turbografxcd", "PCE" }, { "wswan", "WS" }, { "wswanc", "WSC" },
	};
	static systems_cfg c;
	char one[CFG_STR];
	int t, i, k, bad = 0;

	ck(cfg_load_systems("config/systems.cfg", &c), "config/systems.cfg loads");
	for (t = 0; t < (int)(sizeof TO / sizeof TO[0]); t++) {
		const char *on = NULL;
		int shelves = 0;

		for (i = 0; i < c.count; i++) {
			bool reads = !strcasecmp(c.systems[i].folder, TO[t].folder);

			for (k = 0; !reads && cfg_es_at(&c.systems[i], k, one, sizeof one); k++)
				if (!strcasecmp(one, TO[t].folder)) reads = true;
			if (reads) { on = c.systems[i].tag; shelves++; }
		}
		if (shelves == 1 && !strcmp(on, TO[t].tag)) continue;
		if (shelves > 1) printf("    %s reads on %d shelves, wanted %s alone\n", TO[t].folder, shelves, TO[t].tag);
		else printf("    %s reads on %s, wanted %s\n", TO[t].folder, on ? on : "no shelf", TO[t].tag);
		bad++;
	}
	ck(!bad, "playstation, ROCKNIX's every folder and the other systems' read on their shelves");
}

/* tools/from-rogue.sh's table, which copies ROGUE OS's folders into
 * ZyperOS's: each of its game, sets and keep rows names a ROGUE folder
 * that is the shelf's own (in other capitals) or one of its es names, so
 * the card ROGUE OS left reads here as the tool would have copied it */
static void from_rogue_check(void)
{
	static systems_cfg c;
	FILE *f = fopen("tools/from-rogue.sh", "r");
	char line[1024], one[CFG_STR];
	int rows = 0, bad = 0, i, k;

	ck(f != NULL, "tools/from-rogue.sh opens");
	if (!f || !cfg_load_systems("config/systems.cfg", &c)) { if (f) fclose(f); return; }
	while (fgets(line, sizeof line, f)) {
		char *kind = line, *rogue, *place, *bar;
		bool ok = false;

		if (strncmp(line, "game|", 5) && strncmp(line, "sets|", 5) && strncmp(line, "keep|", 5))
			continue;
		rogue = strchr(kind, '|') + 1;
		if (!(bar = strchr(rogue, '|'))) continue;
		*bar = '\0';
		place = bar + 1;
		if (strncmp(place, "Roms/", 5) || !(bar = strchr(place, '|'))) continue;
		*bar = '\0';
		place += 5;
		rows++;
		for (i = 0; i < c.count && !ok; i++) {
			if (strcmp(c.systems[i].folder, place)) continue;
			if (!strcasecmp(c.systems[i].folder, rogue)) ok = true;
			for (k = 0; !ok && cfg_es_at(&c.systems[i], k, one, sizeof one); k++)
				if (!strcmp(one, rogue)) ok = true;
		}
		if (!ok) { printf("    ROGUE's %s (to Roms/%s) is no es name of that shelf\n", rogue, place); bad++; }
	}
	fclose(f);
	ck(rows > 40, "from-rogue.sh's table read");
	ck(!bad, "every ROGUE folder from-rogue.sh copies is its shelf's ES name");
}

int main(void)
{
	static systems_cfg c;
	char dir[] = "/tmp/zyperos-config-XXXXXX", path[64];
	const system_cfg *s;
	FILE *f;

	if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
	snprintf(path, sizeof path, "%s/systems.cfg", dir);
	f = fopen(path, "w");
	if (!f) { perror(path); return 1; }
	fputs("# one line each way a color can be named, or not\n"
	      "sys|Named|named|core|NAM|N.png|C4443A|nes\n"
	      "sys|Lower|lower|core|LOW|L.png|8a6fc0|sfc\n"
	      "sys|Black|black|core|BLK|B.png|000000|bin\n"
	      "sys|Empty|empty|core|EMP|E.png||gb\n"
	      "sys|Spaces|spaces|core|SPC|S.png|   |gb\n"
	      "sys|Short|short|core|SHO|S.png\n"
	      "sys|Word|word|core|WRD|W.png|purple|gb\n"
	      "sys|Five|five|core|FIV|F.png|C4443|gb\n"
	      "sys|Seven|seven|core|SEV|S.png|C4443A0|gb\n"
	      "sys|Hash|hash|core|HSH|H.png|#C4443A|gb\n"
	      "# by tag, before and after the line they add to\n"
	      "map|NAM|a:b,b:a,x:y,y:x,l1:l2,l2:l1,r1:r2,r2:r1\n"
	      "power|NAM|medium\n"
	      "controller|LOW|517\n"
	      "map|NOPE|a:b\n"
	      "sys|Late|late|core|LAT|L.png|C4443A|gb\n"
	      "power|LAT|light\n"
	      "pace|LOW|smooth\n"
	      "pace|LAT|exact\n"
	      "pace|BLK|core\n"
	      "controller|LAT|517\n"
	      "rumble|LAT|35\n"
	      "needs|NAM|gpu-frames\n"
	      "needs|LOW|teleporter\n"
	      "needs|NOPE|gpu-frames\n"
	      "frameskip|WRD|late\n"
	      "frameskip|BLK|always\n"
	      "es|NAM|famicom, fds\n"
	      "sys|Two|two|run:Emulators/Two/Two.sh,core|TWO|T.png|C4443A|gb\n"
	      "brings|TWO|run:Emulators/Two/Two.sh|Bios/Two/a,Bios/Two/b\n"
	      "brings|NAM|run:Emulators/None/None.sh|Bios/x\n", f);
	fclose(f);

	printf("reading:\n");
	ck(cfg_load_systems(path, &c), "the file loads");
	ck(c.count == 12, "every sys line is a system, colored or not");

	printf("a color:\n");
	s = find(&c, "Named");
	ck(s && s->accent == 0xC4443Au, "six hex digits are that color");
	s = find(&c, "Lower");
	ck(s && s->accent == 0x8A6FC0u, "in either case");
	s = find(&c, "Black");
	ck(s && s->accent == 0x000000u, "black is a color, not the lack of one");

	printf("no color:\n");
	ck(CFG_NO_COLOR > 0xFFFFFFu, "CFG_NO_COLOR is no color a line can name");
	s = find(&c, "Empty");
	ck(s && s->accent == CFG_NO_COLOR, "an empty field names none");
	s = find(&c, "Spaces");
	ck(s && s->accent == CFG_NO_COLOR, "nor does one of spaces");
	s = find(&c, "Short");
	ck(s && s->accent == CFG_NO_COLOR, "nor a line that stops before it");
	s = find(&c, "Word");
	ck(s && s->accent == CFG_NO_COLOR, "a word is not a color");
	s = find(&c, "Five");
	ck(s && s->accent == CFG_NO_COLOR, "five digits are not one");
	s = find(&c, "Seven");
	ck(s && s->accent == CFG_NO_COLOR, "nor seven");
	s = find(&c, "Hash");
	ck(s && s->accent == CFG_NO_COLOR, "nor a web color's # (the file never had one)");

	printf("by tag:\n");
	{
		char keys[96];

		s = find(&c, "Named");
		ck(s && !strcmp(s->map, "a:b,b:a,x:y,y:x,l1:l2,l2:l1,r1:r2,r2:r1"),
		   "map| gives its system the map, whole");
		ck(s && !strcmp(s->power, "medium") && s->controller == 0,
		   "beside power|, and no controller unless one is named");
		s = find(&c, "Lower");
		ck(s && s->controller == 517 && !s->map[0] && !s->power[0],
		   "controller| gives the number, and nothing else");
		s = find(&c, "Late");
		ck(s && !strcmp(s->power, "light") && s->controller == 517,
		   "lines after the system they name still reach it");
		s = find(&c, "Black");
		ck(s && !s->map[0] && !s->power[0] && !s->controller,
		   "a system nobody names gets none, and a tag nobody has is dropped");

		s = find(&c, "Named");
		ck(s && s->needs_gpu_frames, "needs| gives its system the need");
		s = find(&c, "Lower");
		ck(s && !s->needs_gpu_frames && s->controller == 517,
		   "a need the launcher does not know is none, and changes nothing else");
		s = find(&c, "Late");
		ck(s && !s->needs_gpu_frames, "a system with no needs line needs nothing");
		cfg_run_keys(find(&c, "Named"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "power=medium"), "a need is no RUN key");

		cfg_run_keys(find(&c, "Black"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, ""), "no RUN keys for a plain system");
		s = find(&c, "Lower");
		ck(s && !strcmp(s->pace, "smooth"), "pace|tag|smooth gives the system Timing, Smooth its own");
		s = find(&c, "Two");
		ck(s && !strcmp(s->brings_core, "run:Emulators/Two/Two.sh") && !strcmp(s->brings, "Bios/Two/a,Bios/Two/b"),
		   "brings| gives the shelf's core the files the player brings");
		ck(s && !strcmp(s->core, "run:Emulators/Two/Two.sh") && cfg_core_count(s) == 2,
		   "and the core field keeps its two cores");
		s = find(&c, "Named");
		ck(s && !s->brings_core[0] && !s->brings[0], "a brings line for a core the shelf lacks is none");
		s = find(&c, "Black");
		ck(s && !s->pace[0], "and a word that is neither smooth nor exact is no line: no Timing row");
		s = find(&c, "Named");
		ck(s && !s->pace[0], "nor has a system with no pace line one");
		cfg_run_keys(find(&c, "Lower"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "controller=517\tpace=smooth"), "controller=, then the system's own pace=");
		cfg_run_keys(find(&c, "Lower"), false, "exact", keys, (int)sizeof keys);
		ck(!strcmp(keys, "controller=517\tpace=exact"), "the player's choice over the system's");
		cfg_run_keys(find(&c, "Lower"), false, "fast", keys, (int)sizeof keys);
		ck(!strcmp(keys, "controller=517\tpace=smooth"), "a choice that is none: the system's own");
		cfg_run_keys(find(&c, "Named"), false, "smooth", keys, (int)sizeof keys);
		ck(!strcmp(keys, "power=medium"), "and no pace= at all where the system has no Timing row");
		s = find(&c, "Late");
		ck(s && s->rumble == 35, "rumble| gives the motor's share, in percent");
		s = find(&c, "Lower");
		ck(s && s->rumble == 0, "and none is 0, the core's own");
		cfg_run_keys(find(&c, "Late"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "power=light\tcontroller=517\trumble=35\tpace=exact"), "power=, controller=, rumble=, pace=");
		cfg_run_keys(find(&c, "Late"), true, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "screens=2\tframeskip=late\tpower=light\tcontroller=517\trumble=35\tpace=exact"),
		   "two screens first, all tab separated");
		/* frameskip=late always: the DS shelf's When Slow is gone (1.0.6), so
		 * a Steady 30 a card kept from before has nothing that sends it */
		cfg_run_keys(find(&c, "Black"), true, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "screens=2\tframeskip=late"), "two screens alone, frameskip=late");
		cfg_run_keys(find(&c, "Black"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, ""), "and one screen sends no frameskip at all");

		/* frameskip|tag|late: the late skip without two screens (the PSP's) */
		s = find(&c, "Word");
		ck(s && s->late_skip, "frameskip|tag|late gives the system the late skip");
		s = find(&c, "Black");
		ck(s && !s->late_skip, "a word that is not late is no line");
		s = find(&c, "Named");
		ck(s && !s->late_skip, "nor has a system with no frameskip line it");
		cfg_run_keys(find(&c, "Word"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "frameskip=late"), "frameskip=late, with one screen");
		cfg_run_keys(find(&c, "Word"), true, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "screens=2\tframeskip=late"), "and two screens send it once, after screens=2");
		cfg_run_keys(find(&c, "Lower"), false, NULL, keys, (int)sizeof keys);
		ck(!strcmp(keys, "controller=517\tpace=smooth"), "a system without the line sends none");
	}

	printf("EmulationStation's folder names:\n");
	ck(cfg_load_systems(path, &c), "the file loads again");
	s = find(&c, "Named");
	{
		char one[CFG_STR];

		ck(s && !strcmp(s->es, "famicom, fds"), "an es line is its tag's list");
		ck(s && cfg_es_at(s, 0, one, sizeof one) && !strcmp(one, "famicom"), "its first name");
		ck(s && cfg_es_at(s, 1, one, sizeof one) && !strcmp(one, "fds"), "its second, spaces dropped");
		ck(s && !cfg_es_at(s, 2, one, sizeof one), "and no third");
		ck(s && !strcmp(s->home, "named"), "home is the folder the line names");
		s = find(&c, "Lower");
		ck(s && !s->es[0] && !cfg_es_at(s, 0, one, sizeof one), "a system with no es line has none");
	}
	unlink(path);
	rmdir(dir);

	/* A to Z (2026-11-06): case and accents do not count, numbers
	 * count as numbers, and the port shelf stays last */
	printf("A to Z:\n");
	ck(cfg_name_cmp("3DO", "32X") < 0, "3DO before 32X: three before thirty-two");
	ck(cfg_name_cmp("Atari 2600", "Atari 5200") < 0 && cfg_name_cmp("Atari 7800", "Atari Lynx") < 0,
	   "the Ataris by number, then Lynx");
	ck(cfg_name_cmp("MAME", "Master System") < 0 && cfg_name_cmp("Master System", "MSX2") < 0,
	   "capitals do not count: MAME, Master System, MSX2");
	ck(cfg_name_cmp("Pokémon Mini", "Ports") < 0 && cfg_name_cmp("PlayStation", "Pokémon Mini") < 0,
	   "an accent does not count: Pokémon between PlayStation and Ports");
	ck(cfg_name_cmp("Neo Geo", "Neo Geo CD") < 0, "a name before the longer one it starts");
	ck(cfg_name_cmp("Game & Watch", "Game Boy") < 0, "Game & Watch before Game Boy");
	ck(cfg_name_cmp("NES", "nes") == 0, "the same name in other capitals is the same");
	ck(cfg_name_cmp("Disc 02", "Disc 2") == 0 && cfg_name_cmp("Disc 9", "Disc 10") < 0,
	   "leading zeros do not count, and 9 before 10");
	{
		static const char *const AZ[] = {
			"3DO", "32X", "Arcade", "Atari 2600", "Atari 5200", "Atari 7800", "Atari Lynx",
			"Atomiswave", "ColecoVision", "Doom", "Dreamcast", "Game & Watch", "Game Boy",
			"Game Boy Advance",
			"Game Boy Color", "Game Gear", "Genesis", "J2ME", "MAME", "Master System",
			"MSX2", "NAOMI", "Neo Geo", "Neo Geo CD", "Neo Geo Pocket",
			"Neo Geo Pocket Color", "NES", "Nintendo 64", "Nintendo DS", "OpenBOR",
			"PICO-8", "PlayStation", "Pokémon Mini", "PSP", "Saturn", "ScummVM", "Sega CD",
			"SG-1000", "SNES", "TIC-80", "TurboGrafx-16", "Vectrex", "Virtual Boy", "WonderSwan",
			"WonderSwan Color", "Ports",
		};
		int i, n = (int)(sizeof AZ / sizeof AZ[0]), same = 1;

		ck(cfg_load_systems("config/systems.cfg", &c), "config/systems.cfg loads");
		cfg_sort_systems(&c);
		ck(c.count == n, "every system in it, once");
		for (i = 0; i < n && i < c.count; i++)
			if (strcmp(c.systems[i].name, AZ[i])) {
				printf("    at %d: %s, wanted %s\n", i, c.systems[i].name, AZ[i]);
				same = 0;
			}
		ck(same, "the shipped systems in A to Z, Ports last");
		{
			char gpu[128] = "";

			for (i = 0; i < c.count; i++)
				if (c.systems[i].needs_gpu_frames) {
					strcat(gpu, c.systems[i].tag);
					strcat(gpu, " ");
				}
			ck(!strcmp(gpu, "AW DC NAOMI N64 PSP SATURN "),
			   "GPU frames for the Atomiswave, Dreamcast, NAOMI, N64, PSP and Saturn, no other");
		}
		{
			char late[128] = "", keys[96];

			/* The late skip by a line: the PSP's (its core leaves a late
			 * frame's drawing out, mk/patches/ppsspp-frontend-frameskip.patch);
			 * the DS has it with its two screens, without one */
			for (i = 0; i < c.count; i++)
				if (c.systems[i].late_skip) {
					strcat(late, c.systems[i].tag);
					strcat(late, " ");
				}
			ck(!strcmp(late, "PSP "), "frameskip=late by a line for the PSP, no other");
			for (i = 0; i < c.count && strcmp(c.systems[i].tag, "PSP"); i++) {}
			if (i < c.count) cfg_run_keys(&c.systems[i], false, NULL, keys, (int)sizeof keys);
			ck(i < c.count && !strncmp(keys, "frameskip=late", 14), "and a PSP game's RUN sends frameskip=late");
		}
		/* Your PICO-8: the player's files, never shipped, for the core
		 * listed first, with fake08 after it for every other card */
		for (i = 0; i < c.count && strcmp(c.systems[i].tag, "PICO8"); i++) {}
		ck(i < c.count && !strcmp(c.systems[i].cores, "run:ZyperOS/pico8/PICO-8.sh,fake08"),
		   "PICO-8: your PICO-8 first, fake08 after it");
		ck(i < c.count && !strcmp(c.systems[i].brings_core, "run:ZyperOS/pico8/PICO-8.sh") &&
		   !strcmp(c.systems[i].brings, "Bios/PICO-8/pico8_64,Bios/PICO-8/pico8.dat"),
		   "and your PICO-8 needs the zip's two files in Bios/PICO-8");
		{
			int k, nb = 0;

			for (k = 0; k < c.count; k++) if (c.systems[k].brings_core[0]) nb++;
			ck(nb == 1, "no other shelf has files the player brings");
		}
		/* and the tags, which settings hang off, went with their names */
		ck(!strcmp(c.systems[0].tag, "3DO") && !strcmp(c.systems[n - 1].tag, "PRT"),
		   "each line moves whole");
	}

	es_names_check();
	es_folders_check();
	pace_lines_check();
	from_rogue_check();

	if (fails) { printf("config: %d FAILED\n", fails); return 1; }
	printf("config: ok\n");
	return 0;
}
