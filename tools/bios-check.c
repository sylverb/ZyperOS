/* A shelf's firmware, judged as its core will judge it (src/biosfile.h).
 *
 * The Saturn's report on 1.0.5: games started to a black screen with no
 * sound. YabaSanshiro looks for nothing but a name, the first of three that
 * is there, and starts whatever that file holds; a 0-byte saturn_bios.bin
 * does it, and wins over a good mpr-17933.bin beside it. The launcher asked
 * the same question (is a name there) and so let it through.
 *
 * This holds: every disc bios name in config/systems.cfg has a size; a
 * 0-byte, a short, a long and a right-size file, and none, in a scratch
 * Bios/; the core's order (a bad first name said beside a good later one,
 * with the one delete that fixes it, a later name used when the first is not
 * there); a folder of the name not
 * taken for the file; two sizes for one name (a System Card's header); and
 * what the shelf says, in English and in each language's own words for a
 * size.
 *
 * Links src/biosfile.c, src/config.c and the strings, and NOT SDL. Writes
 * under a scratch directory of its own.
 */
#include "../src/biosfile.h"
#include "../src/i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static const system_cfg *find(const systems_cfg *c, const char *name)
{
	int i;

	for (i = 0; i < c->count; i++)
		if (!strcmp(c->systems[i].name, name)) return &c->systems[i];
	return NULL;
}

static char dir[] = "/tmp/zyperos-bios-XXXXXX";

/* Bios/<name> with `bytes` bytes, or none at all for -1 */
static void put(const char *name, long bytes)
{
	char path[512];
	FILE *f;
	long i;

	snprintf(path, sizeof path, "%s/%s", dir, name);
	unlink(path);
	if (bytes < 0) return;
	if (!(f = fopen(path, "wb"))) { perror(path); exit(1); }
	for (i = 0; i < bytes; i++) fputc(0x5a, f);
	fclose(f);
}

static void expect(const system_cfg *s, bios_state state, const char *name, unsigned long long have,
                   const char *what)
{
	bios_result r;
	char why[512];

	bios_check(s, dir, &r);
	snprintf(why, sizeof why, "%s (state %d, %s, %llu bytes)", what, (int)r.state, r.name, r.have);
	ck(r.state == state && !strcmp(r.name, name) && (state != BIOS_WRONG_SIZE || r.have == have), why);
}

static void said(const system_cfg *s, const char *want, const char *what)
{
	bios_result r;
	char msg[512], why[1100];

	bios_check(s, dir, &r);
	bios_message(&r, msg, sizeof msg);
	snprintf(why, sizeof why, "%s: \"%s\", not \"%s\"", what, msg, want);
	ck(!strcmp(msg, want), why);
}

/* Every name a shelf's disc bios field or bios line lists has its size */
static void every_name_sized(const systems_cfg *c)
{
	int i, k, named = 0;

	for (i = 0; i < c->count; i++) {
		const system_cfg *s = &c->systems[i];
		const char *p = s->disc_bios;

		for (k = 0; *p; k++) {
			size_t n = strcspn(p, ",");
			char what[CFG_STR + 64];

			snprintf(what, sizeof what, "%s: Bios/%.*s has a firmware line (a size)", s->name, (int)n, p);
			ck(k < CFG_BIOS_NAMES && s->bios_bytes[k][0] > 0, what);
			named++;
			p += n;
			if (*p) p++;
		}
	}
	ck(named >= 12, "the shipped shelves name their firmware");
}

int main(void)
{
	static systems_cfg c;
	const system_cfg *sat, *pce, *ncd, *col;
	char sub[600], out[64];

	ck(cfg_load_systems("config/systems.cfg", &c), "config/systems.cfg loads");
	every_name_sized(&c);
	sat = find(&c, "Saturn");
	pce = find(&c, "TurboGrafx-16");
	ncd = find(&c, "Neo Geo CD");
	col = find(&c, "ColecoVision");
	if (!sat || !pce || !ncd || !col) { printf("  FAIL a shelf is gone from systems.cfg\n"); return 1; }
	ck(sat->bios_bytes[0][0] == 524288 && sat->bios_bytes[1][0] == 524288 && sat->bios_bytes[2][0] == 524288,
	   "the Saturn's three names are 512 KB");
	ck(pce->bios_bytes[0][0] == 262144 && pce->bios_bytes[0][1] == 262656,
	   "a System Card is 256 KB, or 512 bytes more with a header");
	ck(col->bios_every && col->bios_bytes[0][0] == 8192, "the ColecoVision's bios line has its size too");

	if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
	i18n_set(LANG_EN);

	/* none: as before, the first name asked for */
	expect(sat, BIOS_MISSING, "saturn_bios.bin", 0, "no file is missing, the first name");
	said(sat, "needs Bios/saturn_bios.bin", "the missing message is the old one");

	/* the report: a 0-byte first name, good or not later */
	put("saturn_bios.bin", 0);
	expect(sat, BIOS_WRONG_SIZE, "saturn_bios.bin", 0, "a 0-byte file is the wrong size");
	said(sat, "Bios/saturn_bios.bin is 0 bytes; it should be 512 KB", "a 0-byte file is said");
	put("mpr-17933.bin", 524288);
	expect(sat, BIOS_WRONG_SIZE, "saturn_bios.bin", 0,
	       "a bad first name is said beside a good later one (the core uses the first)");
	said(sat, "Bios/saturn_bios.bin is 0 bytes; delete it to use mpr-17933.bin",
	     "and the one delete that fixes it");
	put("sega_101.bin", 100);
	said(sat, "Bios/saturn_bios.bin is 0 bytes; it should be 512 KB",
	     "not when the core would reach a bad second name first");
	put("sega_101.bin", 524288);
	said(sat, "Bios/saturn_bios.bin is 0 bytes; delete it to use sega_101.bin",
	     "a good second name is the one the delete brings");
	put("sega_101.bin", -1);
	put("saturn_bios.bin", 524289);
	said(sat, "Bios/saturn_bios.bin is 524289 bytes; delete it to use mpr-17933.bin",
	     "a size that reads as a good copy's is said in bytes there too");
	put("mpr-17933.bin", -1);
	put("saturn_bios.bin", 10);
	expect(sat, BIOS_WRONG_SIZE, "saturn_bios.bin", 10, "a short file is the wrong size");
	said(sat, "Bios/saturn_bios.bin is 10 bytes; it should be 512 KB", "a short file is said in bytes");
	put("saturn_bios.bin", 262144);
	said(sat, "Bios/saturn_bios.bin is 256 KB; it should be 512 KB", "half a BIOS is said in KB");
	put("saturn_bios.bin", 524289);
	said(sat, "Bios/saturn_bios.bin is 524289 bytes; it should be 524288 bytes",
	     "a size that reads as 512 KB too is said in bytes");
	put("saturn_bios.bin", 524288);
	expect(sat, BIOS_OK, "", 0, "a right-size file starts");
	said(sat, "", "a right-size file says nothing");

	/* a later name, when the first is not there */
	put("saturn_bios.bin", -1);
	put("mpr-17933.bin", 524288);
	expect(sat, BIOS_OK, "", 0, "a later name is used when the first is not there");
	put("mpr-17933.bin", 5000);
	expect(sat, BIOS_WRONG_SIZE, "mpr-17933.bin", 5000, "and is judged as the first would be");
	said(sat, "Bios/mpr-17933.bin is 4 KB; it should be 512 KB", "under 1 MB and not whole KB, round down");
	put("sega_101.bin", 524288);
	expect(sat, BIOS_OK, "", 0, "a good second name is what the core reaches first then");
	put("sega_101.bin", -1);
	put("mpr-17933.bin", -1);

	/* a folder of the name is not the file */
	snprintf(sub, sizeof sub, "%s/saturn_bios.bin", dir);
	ck(mkdir(sub, 0755) == 0, "a scratch folder");
	expect(sat, BIOS_MISSING, "saturn_bios.bin", 0, "a folder of the name is not the file");
	rmdir(sub);

	/* two sizes for one name */
	put("syscard3.pce", 262144);
	expect(pce, BIOS_OK, "", 0, "a System Card of 256 KB");
	put("syscard3.pce", 262656);
	expect(pce, BIOS_OK, "", 0, "and with its 512-byte header");
	put("syscard3.pce", 131072);
	said(pce, "Bios/syscard3.pce is 128 KB; it should be 256 KB", "the first size is the one said");
	put("syscard3.pce", -1);

	/* a name in a folder of Bios/ */
	snprintf(sub, sizeof sub, "%s/neocd", dir);
	ck(mkdir(sub, 0755) == 0, "a scratch neocd folder");
	expect(ncd, BIOS_MISSING, "neocd/neocd.bin", 0, "a name in a folder, missing");
	put("neocd/neocd.bin", 0);
	said(ncd, "Bios/neocd/neocd.bin is 0 bytes; it should be 512 KB", "a name in a folder, said whole");
	put("neocd/neocd.bin", -1);
	rmdir(sub);

	/* sizes as the player reads them */
	bios_size_text(1, out, sizeof out);
	ck(!strcmp(out, "1 byte"), "1 byte");
	bios_size_text(8192, out, sizeof out);
	ck(!strcmp(out, "8 KB"), "8 KB");
	bios_size_text(1048576, out, sizeof out);
	ck(!strcmp(out, "1 MB"), "1 MB, not 1024 KB");
	bios_size_text(2097152, out, sizeof out);
	ck(!strcmp(out, "2 MB"), "2 MB");
	bios_size_text(1572864, out, sizeof out);
	ck(!strcmp(out, "1536 KB"), "not whole MB stays in KB");
	i18n_set(LANG_FR);
	bios_size_text(0, out, sizeof out);
	ck(!strcmp(out, "0 octet"), "French: 0 octet (its one form is 0 and 1)");
	bios_size_text(524288, out, sizeof out);
	ck(!strcmp(out, "512 Ko"), "French: 512 Ko");
	put("saturn_bios.bin", 0);
	said(sat, "Bios/saturn_bios.bin fait 0 octet au lieu de 512 Ko", "the French message");
	i18n_set(LANG_DE);
	said(sat, "Bios/saturn_bios.bin ist 0 Bytes groß, sollte 512 KB sein", "the German message");
	put("os7.u2", 8192);
	put("coleco.rom", 0);
	said(col, "Bios/coleco.rom hat 0 Bytes; lösche sie, um os7.u2 zu nutzen",
	     "the German delete message, on the ColecoVision's third name");
	put("coleco.rom", -1);
	put("os7.u2", -1);
	put("saturn_bios.bin", -1);

	rmdir(dir);
	if (fails) {
		printf("bios-check: %d failed\n", fails);
		return 1;
	}
	printf("bios-check: ok\n");
	return 0;
}
