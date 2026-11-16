/* A save state from the previous DSperate in a built DSperate core
 * (mk/dsperate-state-check.sh, which mk/build-dsperate.sh runs on each core
 * it builds).
 *
 * ZyperOS moved its DS core from DSperate v3.0.5 to v3.4.1, and Diatom hands
 * a v3.0.5 state to the newer core (diatom/src/save.c, STATE_READS) so that
 * players keep their resume points. That rests on v3.4.1 reading v3.0.5's
 * states whole. This loads the core the way Diatom does, boots a ROM it
 * writes itself (no game needed), whose ARM9 counts VBlanks in main RAM, and
 * loads tools/dsperate-state-v305.state into it: a state the v3.0.5 core
 * made of that ROM, 300 frames in, with the count at SAVED. Then:
 *
 *   - the core takes the state;
 *   - main RAM holds the count the state carries, so the machine is the
 *     saved one and not the fresh boot (the count is 0 there);
 *   - 60 frames on, the count is 60 more: the game runs on from the saved
 *     point, the CPUs, the display timing and its events all restored.
 *
 * Exit 0 when all three hold. With --make, it boots the ROM, runs 300 frames
 * and writes the core's state instead: how the file was made, with the
 * v3.0.5 core (2ada9d23..., 2026-11-16); SAVED is the count it printed. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "libretro.h"

#define MAKE_FRAMES 300
#define SAVED       300u        /* the count in tools/dsperate-state-v305.state */
#define COUNT_AT    0x100000u   /* 0x02100000, from the start of main RAM */

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

		/* the interpreter, as mk/dsperate-rtc-check.sh runs it: a
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

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

/* The ARM9's whole program (tools/dsperate-state-check.s): the count at
 * 0x02100000 set to 0, then one more at each VBlank, seen on DISPSTAT. */
static const uint32_t ARM9[] = {
	0xE3A00621, 0xE3A01000, 0xE5801000, 0xE59F2024, 0xE1D230B0, 0xE3130001, 0x1AFFFFFC, 0xE1D230B0,
	0xE3130001, 0x0AFFFFFC, 0xE5901000, 0xE2811001, 0xE5801000, 0xEAFFFFF5, 0x04000004,
};

/* A card: a header, the ARM9's code at 0x8000 (out of the secure area)
 * loaded to main RAM, the ARM7's, one branch to itself, at 0x9000 to its
 * WRAM. DSperate's direct boot needs nothing else. */
static int write_rom(const char *path)
{
	static uint8_t rom[0xA000];
	FILE *f;
	size_t i;

	memcpy(rom, "ZYPEROS STC ZSTC01", 18);
	put32(rom + 0x20, 0x8000);      put32(rom + 0x24, 0x02000000);
	put32(rom + 0x28, 0x02000000);  put32(rom + 0x2C, sizeof ARM9);
	put32(rom + 0x30, 0x9000);      put32(rom + 0x34, 0x037F8000);
	put32(rom + 0x38, 0x037F8000);  put32(rom + 0x3C, 4);
	put32(rom + 0x80, sizeof rom);  put32(rom + 0x84, 0x4000);
	for (i = 0; i < sizeof ARM9 / 4; i++) put32(rom + 0x8000 + 4 * i, ARM9[i]);
	put32(rom + 0x9000, 0xEAFFFFFE);   /* the ARM7: b . */
	f = fopen(path, "wb");
	if (!f || fwrite(rom, 1, sizeof rom, f) != sizeof rom) return -1;
	return fclose(f);
}

static uint8_t *read_file(const char *path, size_t *n)
{
	FILE *f = fopen(path, "rb");
	uint8_t *b = NULL;
	long len;

	*n = 0;
	if (!f) return NULL;
	if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 &&
	    (b = malloc((size_t)len)) && fread(b, 1, (size_t)len, f) == (size_t)len)
		*n = (size_t)len;
	else {
		free(b);
		b = NULL;
	}
	fclose(f);
	return b;
}

int main(int argc, char **argv)
{
	char rom[512];
	void *h;
	struct retro_game_info game;
	const uint8_t *ram;
	uint32_t count;
	int make = argc == 5 && !strcmp(argv[3], "--make"), i;
	const char *path = make ? argv[4] : argv[3];

	if (argc != 4 && !make) {
		fprintf(stderr, "usage: %s core.so workdir state | core.so workdir --make state\n", argv[0]);
		return 2;
	}
	snprintf(sysdir, sizeof sysdir, "%s/system", argv[2]);
	snprintf(savedir, sizeof savedir, "%s/saves", argv[2]);
	snprintf(rom, sizeof rom, "%s/count.nds", argv[2]);
	mkdir(sysdir, 0755);   /* empty: FreeBIOS and a generated firmware */
	mkdir(savedir, 0755);
	if (write_rom(rom)) { perror(rom); return 2; }

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
	SYM(void, retro_get_system_info, (struct retro_system_info *))
	SYM(void, retro_init, (void))
	SYM(bool, retro_load_game, (const struct retro_game_info *))
	SYM(void, retro_run, (void))
	SYM(void, retro_unload_game, (void))
	SYM(size_t, retro_serialize_size, (void))
	SYM(bool, retro_serialize, (void *, size_t))
	SYM(bool, retro_unserialize, (const void *, size_t))
	SYM(void *, retro_get_memory_data, (unsigned))

	retro_set_environment(env);
	retro_set_video_refresh(video);
	retro_set_audio_sample(audio1);
	retro_set_audio_sample_batch(audio);
	retro_set_input_poll(poll_);
	retro_set_input_state(input);
	retro_init();
	{
		struct retro_system_info si;

		retro_get_system_info(&si);
		printf("a DSperate v3.0.5 state in %s %s\n", si.library_name, si.library_version);
	}
	memset(&game, 0, sizeof game);
	game.path = rom;
	if (!retro_load_game(&game)) { printf("  FAIL the core did not load the ROM\n"); return 1; }
	ram = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
	if (!ram) { printf("  FAIL no main RAM from the core\n"); return 1; }

	if (make) {
		size_t n = retro_serialize_size();
		uint8_t *s = malloc(n);
		FILE *f;

		for (i = 0; i < MAKE_FRAMES; i++) retro_run();
		memcpy(&count, ram + COUNT_AT, 4);
		if (!n || !s || !retro_serialize(s, n)) { printf("  FAIL no state\n"); return 1; }
		f = fopen(path, "wb");
		if (!f || fwrite(s, 1, n, f) != n || fclose(f)) { perror(path); return 1; }
		printf("  wrote %zu bytes to %s after %d frames, the count at %u\n", n, path, MAKE_FRAMES, count);
		retro_unload_game();
		return 0;
	}

	{
		size_t n;
		uint8_t *s = read_file(path, &n);

		if (!s) { printf("  FAIL no state at %s\n", path); return 1; }
		if (!retro_unserialize(s, n)) {
			printf("  FAIL the core refused the v3.0.5 state (%zu bytes)\n", n);
			return 1;
		}
		printf("  ok   the core took the v3.0.5 state (%zu bytes)\n", n);
		free(s);
	}
	memcpy(&count, ram + COUNT_AT, 4);
	if (count == SAVED)
		printf("  ok   main RAM is the state's: the VBlank count is %u, as saved\n", count);
	else {
		printf("  FAIL main RAM is not the state's: the VBlank count is %u, not %u\n", count, SAVED);
		fails++;
	}
	for (i = 0; i < 60; i++) retro_run();
	memcpy(&count, ram + COUNT_AT, 4);
	if (count == SAVED + 60)
		printf("  ok   60 frames on, the count is %u: the game runs on from the saved point\n", count);
	else {
		printf("  FAIL 60 frames on, the count is %u, not %u\n", count, SAVED + 60);
		fails++;
	}
	retro_unload_game();

	printf(fails ? "%d FAILED\n" : "all ok\n", fails);
	return fails != 0;
}
