/* SPDX-License-Identifier: GPL-3.0-only */
/* make check-cheatsdl: the cheats download's own rules (src/cheatsdl.h),
 * with no network: a zip's end and index read, the card's collections kept
 * and nothing else, a name that would leave its game's folder refused, a
 * size past a cheat file's refused, a game's files matched (its name, its
 * name with a code type, its title and region in the loose rule, never
 * another game that starts the same), an entry inflated, checked and written
 * where the in-game list reads it, and a system counted from the card.
 *
 * cheatsdl.c is included whole, so its statics are under test; what it calls
 * of the rest of the launcher is stood in for here. */
#include "../src/cheatsdl.c"

#include <ftw.h>

const char *P_CARD, *P_ROMS;
static int failures;

static void ck(bool ok, const char *what)
{
	if (!ok) failures++;
	printf("%s %s\n", ok ? "ok  " : "FAIL", what);
}

/* ---- stand-ins ------------------------------------------------------------------ */

const char *art_lr_collection(const char *folder, int n)
{
	if (!strcmp(folder, "SNES")) return n == 0 ? "Nintendo - Super Nintendo Entertainment System" : NULL;
	if (!strcmp(folder, "NES")) return n == 0 ? "Nintendo - Nintendo Entertainment System" : NULL;
	return NULL;
}

/* libretro's loose rule, as artscrape.c's art_norm: brackets out, letters
 * and digits lowered, the rest one space */
void art_norm(const char *in, char *out, size_t outn)
{
	size_t o = 0;
	int depth = 0;
	bool sp = false;

	for (; *in && o + 1 < outn; in++) {
		char c = *in;

		if (c == '(' || c == '[') { depth++; continue; }
		if ((c == ')' || c == ']') && depth) { depth--; continue; }
		if (depth) continue;
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { if (sp && o) out[o++] = ' '; out[o++] = c; sp = false; }
		else if (c >= 'A' && c <= 'Z') { if (sp && o) out[o++] = ' '; out[o++] = (char)(c - 'A' + 'a'); sp = false; }
		else sp = true;
	}
	out[o] = '\0';
}

bool lib_scan_listing(const char *roms_root, const char *folder, const char *exts, game_list *out)
{
	(void)roms_root; (void)folder; (void)exts; (void)out;
	return false;
}
void lib_free(game_list *l) { free(l->items); l->items = NULL; l->count = 0; }
bool net_get_async_hdr(const char *url, const char *path, int timeout_s, const char *header)
{
	(void)url; (void)path; (void)timeout_s; (void)header;
	return false;
}
int net_async_poll(void) { return -1; }
int net_async_http(void) { return 0; }
int net_async_exit(void) { return -1; }
void net_async_abort(void) { }
net_slot net_slot_use(net_slot s) { (void)s; return NET_SLOT_MAIN; }
bool net_online(void) { return true; }
unsigned plat_now_ms(void) { return 0; }
db *db_lib(void) { return NULL; }
void db_each_prefix(db *d, const char *prefix, db_each_fn fn, void *ctx) { (void)d; (void)prefix; (void)fn; (void)ctx; }
bool db_del(db *d, const char *key) { (void)d; (void)key; return true; }
static int rm_one(const char *p, const struct stat *s, int t, struct FTW *f) { (void)s; (void)t; (void)f; return remove(p); }
bool rm_tree(const char *path) { return nftw(path, rm_one, 16, FTW_DEPTH | FTW_PHYS) == 0; }

/* ---- a zip, made here --------------------------------------------------------------- */

static unsigned char zip[1 << 16];
static size_t zn;
static unsigned char cd[1 << 15];
static size_t cdn;

static void put16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void put32(unsigned char *p, unsigned v) { put16(p, v & 0xffff); put16(p + 2, v >> 16); }

/* An entry: its local header and data at the zip's end, its record in the
 * index; `fake_size` lies about its size in the index (0: the truth) */
static void add(const char *name, const char *text, bool packed, unsigned fake_size)
{
	unsigned char data[4096];
	unsigned len = (unsigned)strlen(text), clen = len, nlen = (unsigned)strlen(name), crc;
	unsigned off = (unsigned)zn;

	crc = (unsigned)crc32(0L, (const Bytef *)text, len);
	if (packed) {
		z_stream z;

		memset(&z, 0, sizeof z);
		deflateInit2(&z, 9, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
		z.next_in = (Bytef *)text;
		z.avail_in = len;
		z.next_out = data;
		z.avail_out = sizeof data;
		deflate(&z, Z_FINISH);
		clen = (unsigned)z.total_out;
		deflateEnd(&z);
	} else {
		memcpy(data, text, len);
	}
	memset(zip + zn, 0, 30);
	put32(zip + zn, 0x04034b50);
	put16(zip + zn + 8, packed ? 8 : 0);
	put32(zip + zn + 14, crc);
	put32(zip + zn + 18, clen);
	put32(zip + zn + 22, len);
	put16(zip + zn + 26, nlen);
	memcpy(zip + zn + 30, name, nlen);
	memcpy(zip + zn + 30 + nlen, data, clen);
	zn += 30 + nlen + clen;
	memset(cd + cdn, 0, 46);
	put32(cd + cdn, 0x02014b50);
	put16(cd + cdn + 10, packed ? 8 : 0);
	put32(cd + cdn + 16, crc);
	put32(cd + cdn + 20, fake_size ? fake_size : clen);
	put32(cd + cdn + 24, fake_size ? fake_size : len);
	put16(cd + cdn + 28, nlen);
	put32(cd + cdn + 42, off);
	memcpy(cd + cdn + 46, name, nlen);
	cdn += 46 + nlen;
}

static void finish_zip(unsigned count)
{
	unsigned off = (unsigned)zn;

	memcpy(zip + zn, cd, cdn);
	zn += cdn;
	memset(zip + zn, 0, 22);
	put32(zip + zn, 0x06054b50);
	put16(zip + zn + 8, count);
	put16(zip + zn + 10, count);
	put32(zip + zn + 12, (unsigned)cdn);
	put32(zip + zn + 16, off);
	zn += 22;
}

static const entry *find(const char *base)
{
	int i;

	for (i = 0; i < g.ne; i++) if (!strcmp(g.e[i].base, base)) return &g.e[i];
	return NULL;
}

static bool matched(const char *base)
{
	int i;

	for (i = 0; i < g.nm; i++) if (!strcmp(g.e[g.match[i]].base, base)) return true;
	return false;
}

int main(void)
{
	static const char *SNES = "Nintendo - Super Nintendo Entertainment System/";
	char card[] = "/tmp/cheatsdl-check-XXXXXX", name[512], path[1024];
	const char *body = "cheats = 2\n\ncheat0_desc = \"Lives\"\ncheat0_code = \"7E0DBE:09\"\n"
	                   "cheat1_desc = \"Moon Jump\"\ncheat1_code = \"7E0072:00\"\n";
	FILE *f;

	if (!mkdtemp(card)) return 1;
	P_CARD = card;
	P_ROMS = card;
#define ADD(n, t, d, s) do { snprintf(name, sizeof name, "%s%s", SNES, n); add(name, t, d, s); } while (0)
	ADD("Super Mario World (USA).cht", body, true, 0);
	ADD("Super Mario World (USA) (Game Genie).cht", body, false, 0);
	ADD("Super Mario World 2 - Yoshi's Island (USA).cht", body, true, 0);
	ADD("Kirby Super Star (Europe).cht", body, true, 0);
	ADD("Kirby Super Star (USA).cht", body, true, 0);
	ADD("../../escape.cht", body, false, 0);
	ADD("sub/folder.cht", body, false, 0);
	ADD(".hidden.cht", body, false, 0);
	ADD("Too Big (USA).cht", body, false, 0xfffffff0u);
	add("Sega - Mega Drive - Genesis/Sonic The Hedgehog (USA).cht", body, true, 0);
	finish_zip(10);

	/* the zip's end says where its index is */
	ck(tail_parse(zip + (zn > TAIL ? zn - TAIL : 0), zn > TAIL ? TAIL : zn) && g.cd_size == cdn && g.total == 10,
	   "the zip's end gives the index's place, size and count");
	ck(!tail_parse((const unsigned char *)"not a zip at all, not one bit", 30), "and something else is no zip's end");

	/* the card's collections, nothing else, and no name that leaves its folder */
	g.ncoll = 0;
	colls_add(art_lr_collection("SNES", 0));
	ck(index_parse(zip + g.cd_off, g.cd_size), "the index reads");
	ck(g.ne == 5, "five of the SNES's entries kept: the Genesis one and the four bad ones left out");
	ck(!find("escape.cht") && !find("../../escape.cht") && !find("folder.cht") && !find(".hidden.cht"),
	   "a name with a folder in it, or hidden, is refused: it would leave the game's folder");
	ck(!find("Too Big (USA).cht"), "a size past a cheat file's is refused");
	ck(!base_ok("..", 2) && !base_ok("a/b", 3) && base_ok("Wario... (USA).cht", 18), "dots inside one name are a name");

	/* matching */
	g.ca = coll_index(art_lr_collection("SNES", 0));
	g.cb = -1;
	match_game("Super Mario World (USA)");
	ck(g.nm == 2 && matched("Super Mario World (USA).cht") && matched("Super Mario World (USA) (Game Genie).cht"),
	   "a ROM's name, and its name with a code type after it");
	ck(!matched("Super Mario World 2 - Yoshi's Island (USA).cht"), "never another game that starts the same");
	match_game("Kirby Super Star (U) [!]");
	ck(g.nm == 1 && matched("Kirby Super Star (USA).cht"), "an old short region tag: (U) is the USA's");
	match_game("Kirby Super Star (Japan)");
	ck(g.nm == 0, "another region's: none (a code is a region's)");
	match_game("kirby super star (USA) [b1]");
	ck(g.nm == 1 && matched("Kirby Super Star (USA).cht"), "the loose rule, the same region");
	match_game("Kirby Super Star");
	ck(g.nm == 2, "with no region on the ROM, every region's");
	match_game("Chrono Trigger (USA)");
	ck(g.nm == 0, "a game libretro has nothing for: none");

	/* an entry: inflated, checked, written where the in-game list reads it */
	snprintf(g.folder, sizeof g.folder, "SNES");
	snprintf(g.rom, sizeof g.rom, "Super Mario World (USA)");
	match_game(g.rom);
	{
		const entry *e = find("Super Mario World (USA).cht"), *s = find("Super Mario World (USA) (Game Genie).cht");
		size_t n;
		unsigned char *b;

		g.got = 0;
		ck(e && entry_take(e, zip + e->off, zn - e->off) == 1, "a deflated entry inflates and its checksum holds");
		ck(s && entry_take(s, zip + s->off, zn - s->off) == 1, "a stored one too");
		ck(g.got == 4, "and their cheats are counted (cheats = N)");
		snprintf(path, sizeof path, "%s/Cheats/SNES/.libretro/Super Mario World (USA)/Super Mario World (USA).cht", card);
		b = read_file(path, &n);
		ck(b && n == strlen(body) && !memcmp(b, body, n), "on the card as Cheats/<folder>/.libretro/<ROM>/<entry>, byte for byte");
		free(b);
		snprintf(path, sizeof path, "%s.part", path);
		ck(access(path, F_OK) != 0, "and no .part left behind");
		{
			unsigned char bad[4096];

			memcpy(bad, zip + e->off, zn - e->off < sizeof bad ? zn - e->off : sizeof bad);
			bad[30 + e->nlen + 2] ^= 0x55;
			ck(entry_take(e, bad, zn - e->off < sizeof bad ? zn - e->off : sizeof bad) == -1,
			   "a damaged entry fails its checksum and writes nothing new");
		}
		ck(!game_wants(), "a game whose files are on the card as the index has them wants nothing");
	}

	/* a game libretro has none for */
	snprintf(g.rom, sizeof g.rom, "Chrono Trigger (USA)");
	match_game(g.rom);
	g.had = false;
	game_none();
	ck(cheats_game_state("SNES", "Chrono Trigger (USA)") == -1, "checked and none: .none in its folder");
	ck(cheats_game_state("SNES", "Super Mario World (USA)") == 1, "files: on the card");
	ck(cheats_game_state("SNES", "F-Zero (USA)") == 0, "no folder: not checked yet");

	/* a system counted from the card */
	{
		game_entry items[3];
		game_list gl;
		cheats_count c;

		memset(&gl, 0, sizeof gl);
		gl.items = items;
		gl.count = 3;
		memset(items, 0, sizeof items);
		snprintf(items[0].name, sizeof items[0].name, "Super Mario World (USA)");
		snprintf(items[1].name, sizeof items[1].name, "Chrono Trigger (USA)");
		snprintf(items[2].name, sizeof items[2].name, "F-Zero (USA)");
		cheats_count_sys("SNES", &gl, &c);
		ck(c.mapped && c.games == 3 && c.have == 1 && c.none == 1 && c.todo == 1, "counted: one with files, one none, one to check");
		cheats_count_sys("Arcade", &gl, &c);
		ck(!c.mapped && !c.todo, "a system libretro has no list for: nothing to check");
	}

	/* your own file is never touched by the delete */
	snprintf(path, sizeof path, "%s/Cheats/SNES/Super Mario World (USA).cht", card);
	if ((f = fopen(path, "w"))) { fputs("cheats = 0\n", f); fclose(f); }
	cheats_delete_sys("SNES");
	ck(access(path, F_OK) == 0 && cheats_game_state("SNES", "Super Mario World (USA)") == 0,
	   "deleting the downloaded ones leaves your own file");

	index_free();
	rm_tree(card);
	if (failures) printf("check-cheatsdl: %d failed\n", failures);
	else printf("ok: the cheats download reads the list, matches, writes and counts as it should\n");
	return failures != 0;
}
