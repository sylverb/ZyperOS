/* SPDX-License-Identifier: GPL-3.0-only */
/* The TrimUI Brick's test kit (src/kit.h; docs/building.md, "Test kits"):
 * linked only into a kit, never into a release. What kit 1.0.7-kit.1's
 * tester runs, from the Brick parity plan:
 *
 *   - Hardware Check, Y on MENU > About (kit_page). Two rows, each started
 *     only by the tester's A, every step logged for the log pack.
 *       Rumble motor: the motor's rail read first and never set (the
 *       AXP2202's BLDO3, which TrimUI's own kernel takes for its motor);
 *       only with it on, PH3 (GPIO 227) high for 300 ms, then low. No node
 *       of the device tree uses the pin; it is made an output that starts
 *       low, so it is never high unasked. Never by itself, and no voltage
 *       is set.
 *       Lights: each LED of the A133's LED controller white alone for 1 s,
 *       in the kernel's order, then all off, so the tester can say which lit
 *       where.
 *   - The AXP2202's registers in the log pack's hardware report
 *     (kit_hw_facts, after src/device/brick_hw.c's own facts), read only,
 *     from its regmap file in debugfs, which the image does not mount:
 *     mounted read-only for the read and unmounted after when it was not
 *     there before. Nothing in debugfs or the PMIC is written.
 *   - Diatom's present times in its log (diatom/port/brick.c,
 *     BRICK_PRESENT_FILE): the file seeded with "auto" at the launcher's
 *     start where there is none. "auto" forces no path; any file turns the
 *     times on.
 *
 * The GPU path's shaders and consoles are on in a kit too, in the Brick's
 * profile (src/device/brick_profile.c).
 *
 * English only, with no strings in res/strings: a kit's page is for its
 * tester and leaves with the kit, so it stays out of the translations and
 * their checks (tools/i18n.py reads S_ IDs, and this file has none). */
#include "../kit.h"
#include "hwfacts.h"
#include "panel.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/mount.h>
#endif

#define HC_GPIO      227        /* PH3: port H (7) x 32 + 3 */
#define HC_PULSE_US  300000
#define HC_LIGHT_US  1000000
#define HC_LEDS      96

typedef struct {
	char motor[96];   /* the motor row's value */
	char lights[96];  /* the lights row's value */
} hc_page;

static int hc_put(const char *path, const char *text)
{
	FILE *f = fopen(path, "w");
	int ok;

	if (!f) return -1;
	ok = fputs(text, f) >= 0 ? 0 : -1;
	if (fclose(f) != 0) ok = -1;
	return ok;
}

static void hc_get(const char *path, char *out, size_t n)
{
	FILE *f = fopen(path, "r");

	out[0] = 0;
	if (f) {
		if (!fgets(out, (int)n, f)) out[0] = 0;
		fclose(f);
	}
	out[strcspn(out, "\n")] = 0;
}

/* BLDO3 by its regulator name: its microvolts and whether it is on */
static bool hc_rail(long *uv, bool *on)
{
	DIR *d = opendir("/sys/class/regulator");
	struct dirent *e;
	bool found = false;

	*uv = 0;
	*on = false;
	while (d && (e = readdir(d))) {
		char path[300], v[48];

		if (e->d_name[0] == '.') continue;
		snprintf(path, sizeof path, "/sys/class/regulator/%s/name", e->d_name);
		hc_get(path, v, sizeof v);
		if (strcmp(v, "axp2202-bldo3")) continue;
		snprintf(path, sizeof path, "/sys/class/regulator/%s/microvolts", e->d_name);
		hc_get(path, v, sizeof v);
		*uv = atol(v);
		snprintf(path, sizeof path, "/sys/class/regulator/%s/state", e->d_name);
		hc_get(path, v, sizeof v);
		*on = !strcmp(v, "enabled");
		found = true;
		break;
	}
	if (d) closedir(d);
	return found;
}

static void hc_motor(hc_page *pg)
{
	char path[64];
	long uv;
	bool on;

	if (!hc_rail(&uv, &on)) {
		fprintf(stderr, "hwcheck: motor: no axp2202-bldo3 regulator: not pulsed\n");
		snprintf(pg->motor, sizeof pg->motor, "No rail found");
		return;
	}
	fprintf(stderr, "hwcheck: motor: rail axp2202-bldo3 %ld uV, %s\n", uv, on ? "enabled" : "off");
	if (!on || uv <= 0) {
		fprintf(stderr, "hwcheck: motor: the rail is off: not pulsed\n");
		snprintf(pg->motor, sizeof pg->motor, "Rail off (%ld uV)", uv);
		return;
	}
	snprintf(path, sizeof path, "/sys/class/gpio/gpio%d/value", HC_GPIO);
	if (access(path, F_OK) != 0) {
		char n[8];

		snprintf(n, sizeof n, "%d", HC_GPIO);
		hc_put("/sys/class/gpio/export", n);
	}
	snprintf(path, sizeof path, "/sys/class/gpio/gpio%d/direction", HC_GPIO);
	/* "low": an output, low, in one step, so the pin is never high unasked */
	if (hc_put(path, "low") != 0) {
		fprintf(stderr, "hwcheck: motor: gpio%d would not be an output: not pulsed\n", HC_GPIO);
		snprintf(pg->motor, sizeof pg->motor, "GPIO %d refused", HC_GPIO);
		return;
	}
	snprintf(path, sizeof path, "/sys/class/gpio/gpio%d/value", HC_GPIO);
	hc_put(path, "1");
	usleep(HC_PULSE_US);
	hc_put(path, "0");
	fprintf(stderr, "hwcheck: motor: PH3 (gpio%d) high for %d ms at %ld uV, then low\n",
	        HC_GPIO, HC_PULSE_US / 1000, uv);
	snprintf(pg->motor, sizeof pg->motor, "Pulsed 300 ms at %.2f V", uv / 1e6);
}

static int hc_name_cmp(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

/* One LED's channels to a brightness: every channel of the LED whose name
 * starts with `stem` and is one letter longer (sunxi_led7r, _7g, _7b), or
 * the one device by that whole name */
static void hc_led(char names[][48], int n, const char *stem, bool on)
{
	size_t len = strlen(stem);
	int i;

	for (i = 0; i < n; i++) {
		char path[128], max[16];

		if (strncmp(names[i], stem, len) || strlen(names[i]) > len + 1) continue;
		snprintf(path, sizeof path, "/sys/class/leds/%s/max_brightness", names[i]);
		hc_get(path, max, sizeof max);
		snprintf(path, sizeof path, "/sys/class/leds/%s/brightness", names[i]);
		hc_put(path, on ? (max[0] ? max : "255") : "0");
	}
}

static void hc_lights(hc_page *pg)
{
	static char names[HC_LEDS][48];
	char stems[HC_LEDS][48];
	DIR *d = opendir("/sys/class/leds");
	struct dirent *e;
	int n = 0, ns = 0, i;

	while (d && (e = readdir(d)) && n < HC_LEDS)
		if (e->d_name[0] != '.') snprintf(names[n++], sizeof names[0], "%.47s", e->d_name);
	if (d) closedir(d);
	qsort(names, (size_t)n, sizeof names[0], hc_name_cmp);
	/* An LED a stem: its name less a last r, g or b, once */
	for (i = 0; i < n; i++) {
		char s[48];
		size_t l = strlen(names[i]);

		snprintf(s, sizeof s, "%.47s", names[i]);
		if (l > 1 && strchr("rgb", s[l - 1])) s[l - 1] = 0;
		if (!ns || strcmp(stems[ns - 1], s)) snprintf(stems[ns++], sizeof stems[0], "%s", s);
	}
	fprintf(stderr, "hwcheck: lights: %d LED devices, %d LEDs\n", n, ns);
	if (!ns) {
		snprintf(pg->lights, sizeof pg->lights, "No LEDs found");
		return;
	}
	for (i = 0; i < n; i++) hc_led(names, n, names[i], false);
	for (i = 0; i < ns; i++) {
		fprintf(stderr, "hwcheck: lights: %d of %d: %s on\n", i + 1, ns, stems[i]);
		hc_led(names, n, stems[i], true);
		usleep(HC_LIGHT_US);
		hc_led(names, n, stems[i], false);
	}
	fprintf(stderr, "hwcheck: lights: all off\n");
	snprintf(pg->lights, sizeof pg->lights, "%d lit one at a time", ns);
}

static int hc_build(void *ctx, menu_row *rows, int max, const char **heading)
{
	hc_page *pg = ctx;

	(void)max;
	*heading = "Hardware Check";
	rows[0] = (menu_row){ "Rumble motor", pg->motor[0] ? pg->motor : "Press A (300 ms)", true };
	rows[1] = (menu_row){ "Lights", pg->lights[0] ? pg->lights : "Press A (one at a time)", true };
	return 2;
}

static menu_result hc_key(app *a, void *ctx, in_button key, int sel)
{
	(void)a;
	if (key != IN_ACCEPT) return MENU_STAY;
	if (sel == 0) hc_motor(ctx);
	else if (sel == 1) hc_lights(ctx);
	return MENU_CHANGED;
}

void kit_page(app *a)
{
	static hc_page pg;

	memset(&pg, 0, sizeof pg);
	fprintf(stderr, "hwcheck: open\n");
	menu_run(a, &(menu_style){ .accent = MENU_ZYPEROS, .fixed_w = menu_std_width(a) },
	         hc_build, hc_key, &pg);
}

void kit_hw_facts(hw_facts *f)
{
#ifdef __linux__
#define DEBUGFS "/sys/kernel/debug"
	bool mounted_here = false;
	char path[300];
	DIR *d;
	struct dirent *e;

	/* Its I2C address is 0x34 (the device tree's pmu@34), so its regmap's
	 * folder ends in "-0034" */
	snprintf(path, sizeof path, "%s/regmap", DEBUGFS);
	if (access(path, F_OK) != 0) {
		if (mount("debugfs", DEBUGFS, "debugfs", MS_RDONLY, NULL) != 0) return;
		mounted_here = true;
	}
	if ((d = opendir(path))) {
		while ((e = readdir(d))) {
			size_t l = strlen(e->d_name);

			if (l < 5 || strcmp(e->d_name + l - 5, "-0034")) continue;
			snprintf(path, sizeof path, "%s/regmap/%s/registers", DEBUGFS, e->d_name);
			hwf_text(path, f->pmic_regs, sizeof f->pmic_regs);
			break;
		}
		closedir(d);
	}
	if (mounted_here) umount2(DEBUGFS, MNT_DETACH);
#undef DEBUGFS
#else
	(void)f;
#endif
}

#ifdef __linux__
/* Diatom's present-time file (diatom/port/brick.c, BRICK_PRESENT_FILE) */
#define KIT_PRESENT_DIR  "/mnt/SDCARD/.userdata/brick"
#define KIT_PRESENT_FILE KIT_PRESENT_DIR "/diatom-present"

/* At the launcher's start, before main: a card whose userdata folder is
 * there and has no such file gets one, once */
__attribute__((constructor)) static void kit_present_seed(void)
{
	FILE *f;

	if (access(KIT_PRESENT_DIR, F_OK) != 0 || access(KIT_PRESENT_FILE, F_OK) == 0) return;
	if (!(f = fopen(KIT_PRESENT_FILE, "w"))) return;
	fputs("auto\n", f);
	if (fclose(f) == 0)
		fprintf(stderr, "kit: %s seeded with \"auto\": Diatom's present times on\n", KIT_PRESENT_FILE);
}
#endif
