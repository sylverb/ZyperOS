/* On-Screen Stats' host side Diatom's ports share: the style from the
 * host's words (diatom_port_hud_style) and the line twice a second
 * (diatom_port_hud), with the chip's temperature, the CPU's load and clock,
 * the battery and the time read for the items asked. The drawing is the
 * port's (gles_draw.h's draw_hud, or the Brick's CPU path). Cut from the GKD
 * Pixel 2's port (port/pixel2.c), which includes it at the place it was cut
 * from, so its program is the same bytes; the TrimUI Brick's port includes
 * it too. What differs, the including port defines first:
 *   HUD_ZONE_A, HUD_ZONE_B  the two thermal zones' types, the hotter shown
 *   HUD_BATTERY_FILE        the battery's capacity in sysfs
 * and the state it writes under g_mx: g_hud_on, g_hud_dirty, g_hud_text,
 * g_hud_colors, g_hud_style. */

/* On-Screen Stats' style from the host (SETHUD), on the emulation thread:
 * the words read into a style once, here, and kept for the line it makes
 * next; a word this does not know keeps the default's */
static hud_style g_hud_style_now;
static bool      g_hud_clock24 = true;

void diatom_port_hud_style(const diatom_hud_words *w)
{
	static const char *const EDGE[HUD_EDGES] = { "close", "normal", "far" };
	static const char *const OPACITY[HUD_OPACITIES] = { "100", "75", "50" };
	hud_style st = hud_default;
	int k;

	st.items = hud_items_parse(w->items);
	if ((k = hud_find(hud_corner_id, HUD_CORNERS, w->corner)) >= 0) st.corner = (uint8_t)k;
	if ((k = hud_find(EDGE, HUD_EDGES, w->edge)) >= 0) st.edge = (uint8_t)k;
	if ((k = hud_find(hud_background_id, HUD_BACKGROUNDS, w->bg)) >= 0) st.background = (uint8_t)k;
	if ((k = hud_find(OPACITY, HUD_OPACITIES, w->opacity)) >= 0) st.opacity = (uint8_t)k;
	if ((k = hud_find(hud_color_id, HUD_COLORS, w->color)) >= 0) st.color = (uint8_t)k;
	if ((k = hud_find(hud_size_id, HUD_SIZES, w->size)) >= 0) st.size = (uint8_t)k;
	hud_clamp(&st);
	g_hud_style_now = st;
	g_hud_clock24 = w->clock24 != 0;
	pthread_mutex_lock(&g_mx);
	g_hud_style = st;
	g_hud_dirty = true;
	pthread_mutex_unlock(&g_mx);
}

static int read_int(const char *path)
{
	char b[32];
	int fd = open(path, O_RDONLY | O_CLOEXEC), v = -1;
	ssize_t n;

	if (fd < 0) return -1;
	n = read(fd, b, sizeof b - 1);
	close(fd);
	if (n > 0) {
		b[n] = 0;
		v = atoi(b);
	}
	return v;
}

/* The chip's hotter zone, in degrees: the SoC's and the GPU's thermal
 * zones, found by their names once, as the launcher's plat_chip_temp */
static int hud_temp_c(void)
{
	static char zone[2][48];
	static int nzones = -1;
	int best = -1, v, i;

	if (nzones < 0) {
		nzones = 0;
		for (i = 0; i < 8 && nzones < 2; i++) {
			char path[64], type[32] = "";
			int fd;
			ssize_t n;

			snprintf(path, sizeof path, "/sys/class/thermal/thermal_zone%d/type", i);
			if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0) continue;
			n = read(fd, type, sizeof type - 1);
			close(fd);
			type[n > 0 ? n : 0] = 0;
			if (strncmp(type, HUD_ZONE_A, sizeof HUD_ZONE_A - 1) && strncmp(type, HUD_ZONE_B, sizeof HUD_ZONE_B - 1)) continue;
			snprintf(zone[nzones++], sizeof zone[0], "/sys/class/thermal/thermal_zone%d/temp", i);
		}
	}
	for (i = 0; i < nzones; i++)
		if ((v = read_int(zone[i])) > best) best = v;
	return best < 0 ? -1 : (best + 500) / 1000;
}

/* Busy against idle since the last time it was asked, from /proc/stat's
 * first line; -1 the first time */
static int hud_cpu_pct(void)
{
	static unsigned long long last_busy, last_all;
	unsigned long long v[10] = { 0 }, all = 0, idle, busy;
	char line[256];
	FILE *f = fopen("/proc/stat", "re");
	int i, pct = -1;

	if (!f) return -1;
	if (!fgets(line, sizeof line, f)) line[0] = 0;
	fclose(f);
	if (sscanf(line, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2],
	           &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]) < 4)
		return -1;
	for (i = 0; i < 8; i++) all += v[i];   /* guest time is in user's already */
	idle = v[3] + v[4];
	busy = all - idle;
	if (last_all && all > last_all)
		pct = (int)((busy - last_busy) * 100 / (all - last_all));
	last_busy = busy;
	last_all = all;
	return pct;
}

/* On-Screen Stats' numbers from the host (diatom_port.h), twice a second:
 * the line made here, on the emulation thread, before g_mx, with what else
 * its items say read now and only those, and the look's colors read as
 * osd_show reads the theme; painted by the render thread */
void diatom_port_hud(bool on, double fps, double speed)
{
	char text[sizeof g_hud_text] = "";
	ds_theme colors;
	const uint8_t items = g_hud_style_now.items ? g_hud_style_now.items : hud_default.items;

	memset(&colors, 0, sizeof colors);
	if (on) {
		hud_values v = { fps, speed, -1, -1, -1, -1, -1, -1, g_hud_clock24 };

		if (items & (1u << HUD_TEMP)) v.temp_c = hud_temp_c();
		if (items & (1u << HUD_CPU)) v.cpu_pct = hud_cpu_pct();
		if (items & (1u << HUD_CLOCK)) {
			int khz = read_int(DIATOM_CPUFREQ "scaling_cur_freq");
			v.cpu_mhz = khz > 0 ? (khz + 500) / 1000 : -1;
		}
		if (items & (1u << HUD_BATTERY)) v.battery_pct = read_int(HUD_BATTERY_FILE);
		if (items & (1u << HUD_TIME)) {
			time_t t = time(NULL);
			struct tm tm;

			if (localtime_r(&t, &tm)) {
				v.hour = tm.tm_hour;
				v.min = tm.tm_min;
			}
		}
		hud_text(text, sizeof text, &v, items);
		themefile_read_bar(THEME_FILE, &colors);
	}
	pthread_mutex_lock(&g_mx);
	if (on && (!g_hud_on || strcmp(text, g_hud_text) ||
	           memcmp(&colors.ground, &g_hud_colors.ground, sizeof colors.ground * 18))) {
		snprintf(g_hud_text, sizeof g_hud_text, "%s", text);
		g_hud_colors = colors;
		g_hud_dirty = true;
	}
	g_hud_on = on && text[0];
	pthread_mutex_unlock(&g_mx);
}
