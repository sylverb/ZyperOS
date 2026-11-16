/* The Nintendo DS clock in a built DSperate core (mk/dsperate-rtc-check.sh,
 * which mk/build-dsperate.sh runs on each core it builds).
 *
 * A DS game reads the time from the console's RTC. DSperate keeps the RTC
 * stopped at 2000-01-01 00:00:00 unless the frontend starts it, and the
 * dsperate-lr wrapper never did, so every game that shows the time showed
 * 0:00 (mk/patches/dsperate-lr-rtc.patch). This loads the core the way Diatom
 * does, boots a ROM of two idle loops it writes itself (no game needed), and
 * reads the RTC's date and time out of the core's own state (DSperate's
 * "IO  " chunk holds the seven BCD bytes a game reads) at each moment the
 * clock could go wrong:
 *
 *   - at the start: the host's local time, in TZ's zone (run it with a zone
 *     other than UTC, as the script does);
 *   - after three host seconds of frames, run as fast as the core goes, so
 *     emulated time is not host time: still the host's time;
 *   - after three seconds with no frame (the frontend's menu, a sleep);
 *   - after a state saved three seconds before is loaded: the host's time,
 *     not the state's;
 *   - after a reset, and in a second game on the same loaded core.
 *
 * A match is the host's local time, at most two seconds behind it or one
 * ahead: the clock counts whole seconds from a time it floors, and the
 * wrapper sets it again once it is a second off. Exit 0 when every one
 * matched. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "libretro.h"

static char sysdir[512], savedir[512];
static int fails;

static void core_log(enum retro_log_level l, const char *fmt, ...)
{
	va_list a;

	(void)l;
	va_start(a, fmt);
	printf("    core: ");
	vprintf(fmt, a);
	va_end(a);
}

static bool env(unsigned cmd, void *data)
{
	switch (cmd) {
	case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: *(const char **)data = sysdir; return true;
	case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:   *(const char **)data = savedir; return true;
	case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:     return true;
	case RETRO_ENVIRONMENT_SET_VARIABLES:        return true;
	case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:      return true;
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *)data)->log = core_log;
		return true;
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable *v = data;

		/* the interpreter: the clock is the same with the JIT, and a
		 * container may not map code */
		if (!strcmp(v->key, "dsperate_jit")) { v->value = "disabled"; return true; }
		return false;
	}
	default: return false;
	}
}

static void video(const void *d, unsigned w, unsigned h, size_t p) { (void)d; (void)w; (void)h; (void)p; }
static void audio1(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio(const int16_t *d, size_t f) { (void)d; return f; }
static void poll_(void) {}
static int16_t input(unsigned a, unsigned b, unsigned c, unsigned d) { (void)a; (void)b; (void)c; (void)d; return 0; }

static void (*c_run)(void);
static size_t (*c_serialize_size)(void);
static bool (*c_serialize)(void *, size_t);
static bool (*c_unserialize)(const void *, size_t);
static uint8_t *state;
static size_t state_n;
static long clock_at = -1;   /* where the clock's seven bytes sit in the IO chunk */

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static uint32_t get32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

/* A card whose two processors each spin on one branch: a header, the ARM9's
 * code at 0x8000 (out of the secure area) loaded to main RAM, the ARM7's at
 * 0x9000 to its WRAM. DSperate's direct boot needs nothing else. */
static int write_rom(const char *path)
{
	static uint8_t rom[0xA000];
	FILE *f;
	int i;

	memcpy(rom, "ZYPEROS RTC ZRTC01", 18);
	put32(rom + 0x20, 0x8000);      put32(rom + 0x24, 0x02000000);
	put32(rom + 0x28, 0x02000000);  put32(rom + 0x2C, 0x10);
	put32(rom + 0x30, 0x9000);      put32(rom + 0x34, 0x037F8000);
	put32(rom + 0x38, 0x037F8000);  put32(rom + 0x3C, 0x10);
	put32(rom + 0x80, sizeof rom);  put32(rom + 0x84, 0x4000);
	for (i = 0; i < 4; i++) {
		put32(rom + 0x8000 + 4 * i, 0xEAFFFFFE);   /* b . */
		put32(rom + 0x9000 + 4 * i, 0xEAFFFFFE);
	}
	f = fopen(path, "wb");
	if (!f || fwrite(rom, 1, sizeof rom, f) != sizeof rom) return -1;
	return fclose(f);
}

static uint8_t bcd(int v) { return (uint8_t)((v / 10 % 10) << 4 | v % 10); }
static int unbcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }

/* The RTC's bytes for a host time, as DSperate writes them (Io::rtc_seed):
 * yy mm dd weekday hh mm ss, BCD, 24-hour. */
static void rtc_bytes(time_t t, uint8_t out[7])
{
	struct tm tm;

	localtime_r(&t, &tm);
	out[0] = bcd(tm.tm_year % 100); out[1] = bcd(tm.tm_mon + 1); out[2] = bcd(tm.tm_mday);
	out[3] = (uint8_t)tm.tm_wday;   out[4] = bcd(tm.tm_hour);    out[5] = bcd(tm.tm_min);
	out[6] = bcd(tm.tm_sec);
}

/* The IO chunk of the state just written: after the length the wrapper puts
 * in front, "DSST", a version, then chunks of a tag and a length. */
static const uint8_t *io_chunk(size_t *len)
{
	const uint8_t *s = state + 4;
	uint32_t n = get32(state), p = 8;

	if (n > state_n - 4 || memcmp(s, "DSST", 4)) return NULL;
	while (p + 8 <= n) {
		uint32_t l = get32(s + p + 4);

		if (p + 8 + l > n) return NULL;
		if (!memcmp(s + p, "IO  ", 4)) { *len = l; return s + p + 8; }
		p += 8 + l;
	}
	return NULL;
}

static void check(const char *what)
{
	static const uint8_t stopped[9] = {0x82, 0, 0, 1, 1, 0, 0, 0, 0};
	const uint8_t *io;
	size_t len, i;
	time_t now;
	uint8_t want[7];
	static const int near[] = {0, -1, 1, -2};
	unsigned k;
	int d;

	if (!c_serialize(state, state_n) || !(io = io_chunk(&len))) {
		printf("  FAIL %s: no state, or no IO chunk in it\n", what);
		fails++;
		return;
	}
	now = time(NULL);
	for (k = 0; k < sizeof near / sizeof near[0]; k++) {
		d = near[k];
		rtc_bytes(now + d, want);
		for (i = 0; i + 7 <= len; i++)
			if (!memcmp(io + i, want, 7)) {
				char at[32];
				struct tm tm;
				time_t t = now + d;

				clock_at = (long)i;
				localtime_r(&t, &tm);
				strftime(at, sizeof at, "%Y-%m-%d %H:%M:%S", &tm);
				printf("  ok   %s: the clock reads %s, the host's local time%s\n", what, at,
				       d == 0 ? "" : d == 1 ? " (a second ahead)" : d == -1 ? " (a second behind)"
				                                                     : " (two seconds behind)");
				return;
			}
	}
	fails++;
	for (i = 0; i + 9 <= len; i++)
		if (!memcmp(io + i, stopped, 9)) {
			printf("  FAIL %s: the clock is stopped at 2000-01-01 00:00:00 (power lost)\n", what);
			return;
		}
	if (clock_at >= 0 && (size_t)clock_at + 7 <= len) {
		const uint8_t *c = io + clock_at;

		printf("  FAIL %s: the clock reads 20%02d-%02d-%02d %02d:%02d:%02d, not the host's local time\n", what,
		       unbcd(c[0]), unbcd(c[1]), unbcd(c[2]), unbcd(c[4] & 0x3F), unbcd(c[5]), unbcd(c[6]));
		return;
	}
	printf("  FAIL %s: no clock at the host's local time in the state\n", what);
}

int main(int argc, char **argv)
{
	char rom[512];
	void *h;
	struct retro_game_info game;
	time_t t;
	long frames;
	uint8_t *saved;
	struct tm lt, ut;

	if (argc != 3) {
		fprintf(stderr, "usage: %s core.so workdir\n", argv[0]);
		return 2;
	}
	snprintf(sysdir, sizeof sysdir, "%s/system", argv[2]);
	snprintf(savedir, sizeof savedir, "%s/saves", argv[2]);
	snprintf(rom, sizeof rom, "%s/clock.nds", argv[2]);
	mkdir(sysdir, 0755);   /* empty: FreeBIOS and a generated firmware */
	mkdir(savedir, 0755);
	if (write_rom(rom)) { perror(rom); return 2; }
	t = time(NULL);
	localtime_r(&t, &lt);
	gmtime_r(&t, &ut);
	if (lt.tm_hour == ut.tm_hour) {
		fprintf(stderr, "run it in a zone other than UTC (TZ), or a core reading UTC would pass\n");
		return 2;
	}
	printf("the DS clock, in zone %s (UTC%+ld h)\n", getenv("TZ") ? getenv("TZ") : "?", lt.tm_gmtoff / 3600);

	h = dlopen(argv[1], RTLD_NOW);
	if (!h) { printf("  FAIL dlopen: %s\n", dlerror()); return 1; }
#define SYM(ret, name, args) ret (*name)args = (ret (*)args)dlsym(h, #name); \
	if (!name) { printf("  FAIL no %s in the core\n", #name); return 1; }
	SYM(void, retro_set_environment, (retro_environment_t))
	SYM(void, retro_set_video_refresh, (retro_video_refresh_t))
	SYM(void, retro_set_audio_sample, (retro_audio_sample_t))
	SYM(void, retro_set_audio_sample_batch, (retro_audio_sample_batch_t))
	SYM(void, retro_set_input_poll, (retro_input_poll_t))
	SYM(void, retro_set_input_state, (retro_input_state_t))
	SYM(void, retro_init, (void))
	SYM(bool, retro_load_game, (const struct retro_game_info *))
	SYM(void, retro_run, (void))
	SYM(void, retro_reset, (void))
	SYM(void, retro_unload_game, (void))
	SYM(size_t, retro_serialize_size, (void))
	SYM(bool, retro_serialize, (void *, size_t))
	SYM(bool, retro_unserialize, (const void *, size_t))
	c_run = retro_run;
	c_serialize_size = retro_serialize_size;
	c_serialize = retro_serialize;
	c_unserialize = retro_unserialize;

	retro_set_environment(env);
	retro_set_video_refresh(video);
	retro_set_audio_sample(audio1);
	retro_set_audio_sample_batch(audio);
	retro_set_input_poll(poll_);
	retro_set_input_state(input);
	retro_init();
	memset(&game, 0, sizeof game);
	game.path = rom;
	if (!retro_load_game(&game)) { printf("  FAIL the core did not load the ROM\n"); return 1; }
	state_n = c_serialize_size();
	state = malloc(state_n);
	saved = malloc(state_n);
	if (!state_n || !state || !saved) { printf("  FAIL no state size\n"); return 1; }

	c_run();
	check("at the start");

	for (t = time(NULL), frames = 0; time(NULL) < t + 3; frames++) c_run();
	{
		char what[96];

		snprintf(what, sizeof what, "after %ld frames in 3 s (%.1f emulated s)", frames, frames / 59.8261);
		check(what);
	}

	sleep(3);
	c_run();
	check("after 3 s with no frame");

	if (!c_serialize(saved, state_n)) { printf("  FAIL no state to load back\n"); return 1; }
	sleep(3);
	if (!c_unserialize(saved, state_n)) { printf("  FAIL the core refused its own state\n"); fails++; }
	check("a state saved 3 s before, loaded");
	c_run();
	check("and a frame after it");

	retro_reset();
	check("after a reset");

	retro_unload_game();
	sleep(2);
	if (!retro_load_game(&game)) { printf("  FAIL the core did not load the ROM again\n"); return 1; }
	state_n = c_serialize_size();
	free(state);
	state = malloc(state_n);
	if (!state_n || !state) { printf("  FAIL no state size\n"); return 1; }
	c_run();
	check("a second game on the loaded core");
	retro_unload_game();

	printf(fails ? "%d FAILED\n" : "all ok\n", fails);
	return fails != 0;
}
