/* SPDX-License-Identifier: GPL-3.0-only */
/* A device's power steps as data (diatom_port_power, diatom_port.h): each
 * clock's ceiling file and its value at full, medium and light power, and
 * the GPU's lowest-clock file diatom_port_fast_clock holds at its top step.
 * A port states its own and writes them with diatom_power_set, so the clocks
 * are numbers in one table and the writing is one piece of code for every
 * port. config/systems.cfg says only "light" and "medium"; each device maps
 * them to its own clocks here. A port whose clocks are not measured yet has
 * none (the Brick's), and its diatom_port_power does nothing. */
#ifndef DIATOM_PORT_POWER_H
#define DIATOM_PORT_POWER_H

#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "diatom_port.h"      /* DIATOM_POWER_FULL, _MEDIUM, _LIGHT */
#include "port_cpufreq.h"     /* diatom_cpufreq_copy */

/* A ceiling: the file, its value at each level (DIATOM_POWER_FULL, _MEDIUM,
 * _LIGHT), and for a value of NULL the file whose value is copied into it
 * instead (the CPU's cpuinfo_max_freq, its own top, for full power) */
typedef struct {
	const char *path;
	const char *at[3];
	const char *top;
} diatom_clock;

typedef struct {
	diatom_clock clock[4];
	int n;
	/* The GPU's lowest clock, held at `gpu_floor_top` while a core draws
	 * with it, "0" (the governor's again) after; NULL for none */
	const char *gpu_floor, *gpu_floor_top;
} diatom_power_profile;

/* `v` into the sysfs file `path`, as the kernel takes it: one write, no
 * truncation, no new file */
static inline bool diatom_sysfs_put(const char *path, const char *v)
{
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	bool ok;

	if (fd < 0) return false;
	ok = write(fd, v, strlen(v)) == (ssize_t)strlen(v);
	close(fd);
	return ok;
}

/* Every ceiling of `p` at `level`, in its order, each tried whatever the
 * last one did; true when every write took */
static inline bool diatom_power_set(const diatom_power_profile *p, int level)
{
	bool ok = true;
	int i;

	if (level < DIATOM_POWER_FULL || level > DIATOM_POWER_LIGHT) return false;
	for (i = 0; i < p->n; i++) {
		const diatom_clock *c = &p->clock[i];

		ok &= c->at[level] ? diatom_sysfs_put(c->path, c->at[level])
		                   : diatom_cpufreq_copy(c->top, c->path);
	}
	return ok;
}

/* The level a port wrote last, so a level said again writes nothing: the host
 * says a level at a game's start and end, around the in-game menu and around
 * fast forward, and a DDR ceiling written on the Pixel 2 waits for the
 * display's vertical blank. -1 is not known: the next level is written,
 * whatever it is.
 *
 * Forgotten at each handover (diatom_port_power_invalidate), because the files
 * are not Diatom's alone: the launcher has them between games and while its
 * menu is up, and on the Pixel 2 it puts the DDR's ceiling at 328 MHz on a
 * screen left still for 2 s (ZyperOS's plat_ram_idle), and lifts it on a
 * press. Remembered across that, a resident Diatom took FULL at its start as
 * written for good, said FULL again at a game's start and wrote nothing, so a
 * game started with no press after a still shelf ran with the DDR at 328 MHz:
 * Mario Kart DS at 20 to 28 fps instead of 59.8 (2026-11-16). */
typedef struct {
	int level;
} diatom_power_memo;

/* `level` written unless it is the one written last; true when it was
 * already, or when every write took. A level outside the three is nothing. */
static inline bool diatom_power_say(diatom_power_memo *m, const diatom_power_profile *p, int level)
{
	if (level < DIATOM_POWER_FULL || level > DIATOM_POWER_LIGHT || level == m->level) return true;
	m->level = level;
	return diatom_power_set(p, level);
}

static inline void diatom_power_forget(diatom_power_memo *m) { m->level = -1; }

/* The GPU's floor held at its top step, or let go */
static inline bool diatom_power_hold_gpu(const diatom_power_profile *p, bool on)
{
	return !p->gpu_floor || diatom_sysfs_put(p->gpu_floor, on ? p->gpu_floor_top : "0");
}

#endif /* DIATOM_PORT_POWER_H */
