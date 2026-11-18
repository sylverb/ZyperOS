/* SPDX-License-Identifier: GPL-3.0-only */
/* Core Settings helpers (src/coreopt_defs.h): OPTION line parse, whitelist
 * empty = show all, value cycle, and the library key shape. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/coreopt_defs.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static void parse_and_key(void)
{
	codef o;
	char key[80];

	printf("OPTION lines and db keys:\n");
	ck(codef_parse_line(
	       "OPTION\tkey=bluemsx_additional_cart\tvalue=scc\tdefault=disabled\t"
	       "values=disabled|scc|sccplus\tdesc=Additional MSX Sound Cartridge (Restart)",
	       &o),
	   "a full OPTION line parses");
	ck(!strcmp(o.key, "bluemsx_additional_cart") && !strcmp(o.value, "scc") &&
	   !strcmp(o.deflt, "disabled") && o.nvals == 3 && !strcmp(o.vals[1], "scc") &&
	   strstr(o.desc, "Restart"),
	   "key, value, default, values and desc");
	ck(!codef_parse_line("OPTIONS\tcount=2", &o), "OPTIONS count is not an OPTION");
	codef_db_key("MSX2", "bluemsx_additional_cart", key, sizeof key);
	ck(!strcmp(key, "coreopt.MSX2.bluemsx_additional_cart"),
	   "db key is coreopt.<TAG>.<option>");
}

static void seed_and_cycle(void)
{
	codefs s;
	int i;

	printf("blueMSX seed and cycle:\n");
	ck(codefs_seed(&s, "bluemsx") && s.n == 3, "bluemsx seeds three options");
	ck(!strcmp(s.opt[0].key, "bluemsx_msxtype") &&
	   !strcmp(s.opt[1].key, "bluemsx_cartmapper") &&
	   !strcmp(s.opt[2].key, "bluemsx_additional_cart"),
	   "machine type, mapper and additional sound cart");
	ck(codef_value_index(&s.opt[0], "MSXturboR") >= 0 &&
	   codef_value_index(&s.opt[2], "disabled") >= 0 &&
	   codef_value_index(&s.opt[2], "scc") >= 0 &&
	   codef_value_index(&s.opt[2], "sccplus") >= 0 &&
	   codef_value_index(&s.opt[1], "Auto") >= 0,
	   "MSXturboR, disabled, scc, sccplus and Auto are among the values");
	ck(!strcmp(s.opt[2].value, "disabled") && !strcmp(s.opt[2].deflt, "disabled"),
	   "additional cart seed stays off");
	i = codef_value_index(&s.opt[2], "scc");
	ck(!strcmp(s.opt[2].vals[codef_value_step(&s.opt[2], i, 1)], "sccexpanded") ||
	   codef_value_step(&s.opt[2], i, 1) != i,
	   "stepping moves to another value");
	ck(!codefs_seed(&s, "gambatte"), "another core has no seed");
}

static void whitelist(void)
{
	char path[] = "/tmp/zyperos-coreopts-showXXXXXX";
	char allow[CODEFS_ALLOW_MAX][CODEFS_KEY_MAX];
	codefs all, shown;
	int fd, n;
	FILE *f;

	printf("whitelist:\n");
	fd = mkstemp(path);
	ck(fd >= 0, "a temp show file");
	if (fd < 0) return;
	close(fd);
	f = fopen(path, "w");
	fprintf(f, "# comment\ncore=bluemsx\nkey=bluemsx_msxtype\n"
	           "key=bluemsx_cartmapper\nkey=bluemsx_additional_cart\n"
	           "core=gambatte\n");
	fclose(f);

	codefs_seed(&all, "bluemsx");
	/* invent an extra so filter is visible */
	if (all.n < CODEFS_MAX) {
		codef *o = &all.opt[all.n++];

		memset(o, 0, sizeof *o);
		snprintf(o->key, sizeof o->key, "bluemsx_other");
		codef_set_values(o, "a|b");
	}
	n = codefs_load_allow(path, "bluemsx", allow, CODEFS_ALLOW_MAX);
	ck(n == 3 && !strcmp(allow[0], "bluemsx_msxtype") &&
	   !strcmp(allow[1], "bluemsx_cartmapper") &&
	   !strcmp(allow[2], "bluemsx_additional_cart"),
	   "three keys under bluemsx");
	codefs_filter(&all, &shown, allow, n);
	ck(shown.n == 3 && !strcmp(shown.opt[0].key, "bluemsx_msxtype") &&
	   !strcmp(shown.opt[2].key, "bluemsx_additional_cart"),
	   "filter keeps only the listed keys");

	n = codefs_load_allow(path, "gambatte", allow, CODEFS_ALLOW_MAX);
	ck(n == 0, "a core with no key= lines allows all");
	codefs_filter(&all, &shown, allow, n);
	ck(shown.n == all.n, "empty allow list passes every definition through");

	n = codefs_load_allow("/no/such/coreopts-show.cfg", "bluemsx", allow, CODEFS_ALLOW_MAX);
	ck(n == 0, "a missing file allows all");
	unlink(path);
}

static void cache_roundtrip(void)
{
	char path[] = "/tmp/zyperos-coreopt-defsXXXXXX";
	codefs a, b;
	int fd;

	printf("cache file:\n");
	fd = mkstemp(path);
	ck(fd >= 0, "a temp cache file");
	if (fd < 0) return;
	close(fd);
	codefs_seed(&a, "bluemsx");
	ck(codefs_save_file(&a, path) && codefs_load_file(&b, path) && b.n == a.n &&
	   !strcmp(b.opt[0].key, a.opt[0].key) && !strcmp(b.opt[1].pipe, a.opt[1].pipe),
	   "save and load keep keys and values");
	unlink(path);
}

int main(void)
{
	parse_and_key();
	seed_and_cycle();
	whitelist();
	cache_roundtrip();
	printf("%d checks, %d failed\n", checks, fails);
	return fails ? 1 : 0;
}
