/* SPDX-License-Identifier: GPL-3.0-only */
/* A port's power steps as data (port/port_power.h): each level writes each
 * ceiling's value, in order, the CPU's full power copied from its own top,
 * the GPU's floor held and let go; a file that cannot be written fails the
 * set and leaves the others written. Into a scratch folder, no device. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "port_power.h"

static int fails, checks;

static void ck(int cond, const char *what)
{
	checks++;
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

static char dir[] = "/tmp/diatom-power-XXXXXX";
static char p_cpu[64], p_top[64], p_ram[64], p_gpu[64], p_floor[64];

/* `path` made again, empty, as a sysfs file is there to be written */
static void fresh(const char *path)
{
	FILE *f = fopen(path, "w");

	if (f) fclose(f);
}

static void put(const char *path, const char *v)
{
	FILE *f = fopen(path, "w");

	if (f) { fputs(v, f); fclose(f); }
}

static int is(const char *path, const char *want)
{
	char v[64] = "";
	FILE *f = fopen(path, "r");

	if (!f) return 0;
	if (!fgets(v, sizeof v, f)) v[0] = '\0';
	fclose(f);
	return !strcmp(v, want);
}

int main(void)
{
	diatom_power_profile p;

	if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
	snprintf(p_cpu, sizeof p_cpu, "%s/scaling_max_freq", dir);
	snprintf(p_top, sizeof p_top, "%s/cpuinfo_max_freq", dir);
	snprintf(p_ram, sizeof p_ram, "%s/dmc_max_freq", dir);
	snprintf(p_gpu, sizeof p_gpu, "%s/gpu_max_freq", dir);
	snprintf(p_floor, sizeof p_floor, "%s/gpu_min_freq", dir);
	put(p_top, "1296000\n");
	memset(&p, 0, sizeof p);
	p.clock[0] = (diatom_clock){ p_cpu, { NULL, "1008000", "816000" }, p_top };
	p.clock[1] = (diatom_clock){ p_ram, { "0", "0", "328000000" }, NULL };
	p.clock[2] = (diatom_clock){ p_gpu, { "0", "300000000", "200000000" }, NULL };
	p.n = 3;
	p.gpu_floor = p_floor;
	p.gpu_floor_top = "520000000";

	printf("power steps:\n");
	fresh(p_cpu); fresh(p_ram); fresh(p_gpu);
	ck(diatom_power_set(&p, DIATOM_POWER_LIGHT), "light power sets");
	ck(is(p_cpu, "816000") && is(p_ram, "328000000") && is(p_gpu, "200000000"),
	   "each ceiling at its light value");
	fresh(p_cpu); fresh(p_ram); fresh(p_gpu);
	ck(diatom_power_set(&p, DIATOM_POWER_MEDIUM), "medium power sets");
	ck(is(p_cpu, "1008000") && is(p_ram, "0") && is(p_gpu, "300000000"), "each at its medium value");
	fresh(p_cpu); fresh(p_ram); fresh(p_gpu);
	ck(diatom_power_set(&p, DIATOM_POWER_FULL), "full power sets");
	ck(is(p_cpu, "1296000\n") && is(p_ram, "0") && is(p_gpu, "0"),
	   "the CPU at its own top, the others with no ceiling");
	ck(!diatom_power_set(&p, 3) && !diatom_power_set(&p, -1), "no level past the three");

	unlink(p_ram);
	fresh(p_cpu); fresh(p_gpu);
	ck(!diatom_power_set(&p, DIATOM_POWER_LIGHT), "a ceiling that cannot be written fails the set");
	ck(is(p_cpu, "816000") && is(p_gpu, "200000000"), "and the others are still written");

	fresh(p_floor);
	ck(diatom_power_hold_gpu(&p, true) && is(p_floor, "520000000"), "the GPU's floor held at its top");
	fresh(p_floor);
	ck(diatom_power_hold_gpu(&p, false) && is(p_floor, "0"), "and let go");
	p.gpu_floor = NULL;
	ck(diatom_power_hold_gpu(&p, true), "no floor to hold is nothing to fail");

	unlink(p_cpu); unlink(p_top); unlink(p_gpu); unlink(p_floor);
	rmdir(dir);
	if (fails) {
		printf("power: %d of %d checks failed\n", fails, checks);
		return 1;
	}
	printf("power: %d checks passed\n", checks);
	return 0;
}
