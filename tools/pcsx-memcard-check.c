/* The PlayStation's memory card across a resume, in a built PCSX ReARMed core
 * (mk/pcsx-memcard-check.sh, which mk/build-pcsx-rearmed.sh runs on the core
 * it builds).
 *
 * ZyperOS starts every game from its auto state. Diatom loads the game's .srm,
 * the memory card in slot 1, and then the state (diatom/src/save.c), and at
 * the exit it writes the .srm and then the state: the card a state is loaded
 * with is the card it was made with. PCSX ReARMed's buildbot core marks both
 * cards "new" after every state load all the same (libpcsxcore/sio.c,
 * sioFreeze, since upstream 886a15b), so at its next card access the game
 * sees another card put in. Rayman answered with "THE MEMORY CARD HAS CHANGED"
 * and its save was lost (Discord bug report, ZyperOS 1.0.5).
 *
 * This loads the core the way Diatom does and boots a program it writes
 * itself (tools/pcsx-memcard-check.s, no game or BIOS needed), which reads the
 * card's FLAG byte on the SIO, writes a sector, and then reads FLAG on, the
 * way a game's card library does. Then:
 *
 *   - played: FLAG is 8 (a new card) at power-on and 0 after the write, and
 *     the write is on the card. The state and the .srm are kept, the .srm
 *     first, as Diatom does at the exit.
 *   - resumed: the .srm and then the state, as Diatom does at the start, and
 *     the game reads FLAG 0: the card it saved to, not a new one.
 *   - resumed over a changed card (another sector written since the state):
 *     FLAG is 8, a new card, so the game reads the card again and does not
 *     write it from what it had in RAM. That is upstream's reason for the
 *     flag, and it holds.
 *
 * With other.so, the other build (the buildbot's, when core.so is the
 * patched one): a state it made loads in core.so, and one core.so made loads
 * in it, and the game runs on from it. FLAG is 8 there, as the buildbot core
 * always had it, since one side has no record of the card.
 *
 * Each in a process of its own, as a game is. Exit 0 when all hold. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "libretro.h"

#define AT      0x100000u       /* what the program leaves, from main RAM's start */
#define MAGIC   0x5444434Du     /* "MCDT" */
#define SECTOR  0x3F            /* the one the program writes */
#define NEW     8u              /* FLAG's "new card" */

static char sysdir[600], savedir[600], exe[600];
static int fails;

static void core_log(enum retro_log_level l, const char *fmt, ...)
{
	va_list a;

	if (l < RETRO_LOG_WARN) return;
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
	case RETRO_ENVIRONMENT_GET_CAN_DUPE:         *(bool *)data = true; return true;
	case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
		((struct retro_log_callback *)data)->log = core_log;
		return true;
	case RETRO_ENVIRONMENT_GET_VARIABLE: {
		struct retro_variable *v = data;

		/* Slot 1 is the frontend's, as Diatom has it: the .srm */
		if (!strcmp(v->key, "pcsx_rearmed_memcard1")) { v->value = "libretro"; return true; }
		if (!strcmp(v->key, "pcsx_rearmed_memcard2")) { v->value = "none"; return true; }
		/* No BIOS file: the core's own */
		if (!strcmp(v->key, "pcsx_rearmed_bios"))     { v->value = "HLE"; return true; }
		/* The interpreter: a container may not map code */
		if (!strcmp(v->key, "pcsx_rearmed_drc"))      { v->value = "disabled"; return true; }
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

static uint32_t get32(const uint8_t *p)
{
	return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* tools/pcsx-memcard-check.s, assembled */
static const uint32_t PROGRAM[] = {
	0x40806000, 0x00000000, 0x3C108010, 0x3C111F80, 0x36311040, 0x3C085444, 0x3508434D, 0xAE080000,
	0xAE000004, 0xAE000010, 0x04110018, 0x00000000, 0xAE020008, 0x34080001, 0xAE080004, 0x0411001F,
	0x00000000, 0x04110011, 0x00000000, 0xAE02000C, 0x34080002, 0xAE080004, 0x0411000C, 0x00000000,
	0xAE020014, 0x8E080010, 0x00000000, 0x25080001, 0xAE080010, 0x3C090001, 0x2529FFFF, 0x1520FFFE,
	0x00000000, 0x1000FFF4, 0x00000000, 0x34080003, 0xA628000A, 0x34080081, 0xA2280000, 0x92290000,
	0x34080052, 0xA2280000, 0x92220000, 0x00000000, 0xA620000A, 0x03E00008, 0x00000000, 0x34080003,
	0xA628000A, 0x34080081, 0xA2280000, 0x92290000, 0x34080057, 0xA2280000, 0x92290000, 0xA2200000,
	0x92290000, 0xA2200000, 0x92290000, 0xA2200000, 0x92290000, 0x3408003F, 0xA2280000, 0x92290000,
	0x340800A5, 0x340A0080, 0xA2280000, 0x92290000, 0x254AFFFF, 0x1540FFFC, 0x00000000, 0x3408003F,
	0xA2280000, 0x92290000, 0xA2200000, 0x92290000, 0xA2200000, 0x92290000, 0xA2200000, 0x92290000,
	0x00000000, 0xA620000A, 0x03E00008, 0x00000000,
};

/* A PS-EXE: the header, then the program, loaded and started at 0x80010000 */
static int write_exe(const char *path)
{
	static uint8_t f[0x1000];
	FILE *o;
	size_t i;

	memcpy(f, "PS-X EXE", 8);
	put32(f + 0x10, 0x80010000);            /* pc0 */
	put32(f + 0x18, 0x80010000);            /* t_addr */
	put32(f + 0x1C, 0x800);                 /* t_size */
	put32(f + 0x30, 0x801FFF00);            /* s_addr */
	memcpy(f + 0x4C, "Sony Computer Entertainment Inc. for North America area", 55);
	for (i = 0; i < sizeof PROGRAM / 4; i++) put32(f + 0x800 + 4 * i, PROGRAM[i]);
	o = fopen(path, "wb");
	if (!o || fwrite(f, 1, sizeof f, o) != sizeof f) return -1;
	return fclose(o);
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

static int write_file(const char *path, const void *d, size_t n)
{
	FILE *f = fopen(path, "wb");

	if (!f || fwrite(d, 1, n, f) != n) return -1;
	return fclose(f);
}

static void check(int ok, const char *fmt, ...)
{
	va_list a;

	printf(ok ? "  ok   " : "  FAIL ");
	va_start(a, fmt);
	vprintf(fmt, a);
	va_end(a);
	printf("\n");
	if (!ok) fails++;
}

static void *h;
static uint8_t *ram, *card;
static size_t card_n;
static void (*run)(void);

#define SYM(ret, name, args) ret (*name)args = (ret (*)args)dlsym(h, #name); \
	if (!name) { printf("  FAIL no %s in the core\n", #name); return 1; }

/* The core, up with the program loaded: what Diatom does before a state */
static int boot(void)
{
	struct retro_game_info game;

	h = dlopen(getenv("CORE"), RTLD_NOW);
	if (!h) { printf("  FAIL dlopen: %s\n", dlerror()); return 1; }
	{
		SYM(void, retro_set_environment, (retro_environment_t))
		SYM(void, retro_set_video_refresh, (retro_video_refresh_t))
		SYM(void, retro_set_audio_sample, (retro_audio_sample_t))
		SYM(void, retro_set_audio_sample_batch, (retro_audio_sample_batch_t))
		SYM(void, retro_set_input_poll, (retro_input_poll_t))
		SYM(void, retro_set_input_state, (retro_input_state_t))
		SYM(void, retro_init, (void))
		SYM(bool, retro_load_game, (const struct retro_game_info *))
		SYM(void, retro_run, (void))
		SYM(void *, retro_get_memory_data, (unsigned))
		SYM(size_t, retro_get_memory_size, (unsigned))

		retro_set_environment(env);
		retro_set_video_refresh(video);
		retro_set_audio_sample(audio1);
		retro_set_audio_sample_batch(audio);
		retro_set_input_poll(poll_);
		retro_set_input_state(input);
		retro_init();
		memset(&game, 0, sizeof game);
		game.path = exe;
		if (!retro_load_game(&game)) { printf("  FAIL the core did not load the program\n"); return 1; }
		run = retro_run;
		ram = retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM);
		card = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
		card_n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
	}
	if (!ram) { printf("  FAIL no main RAM from the core\n"); return 1; }
	if (!card || card_n != 128 * 1024) {
		printf("  FAIL no memory card in slot 1 from the core (%zu bytes)\n", card_n);
		return 1;
	}
	return 0;
}

static uint32_t at(unsigned off) { return get32(ram + AT + off); }

static int sector_is(uint8_t v)
{
	size_t i;

	for (i = 0; i < 128; i++)
		if (card[SECTOR * 128 + i] != v) return 0;
	return 1;
}

/* The state and the .srm a run of `tag` keeps */
static char dir[512];

static void files(const char *tag, char *state, char *srm)
{
	snprintf(state, 600, "%s/%s.state", dir, tag);
	snprintf(srm, 600, "%s/%s.srm", dir, tag);
}

/* Power-on to a saved card, kept as Diatom keeps it at the exit: the .srm,
 * then the state */
static int played(const char *tag, int how)
{
	SYM(size_t, retro_serialize_size, (void))
	SYM(bool, retro_serialize, (void *, size_t))
	char statef[600], srmf[600];
	size_t n;
	uint8_t *s;
	int i;

	(void)how;
	for (i = 0; i < 120 && at(4) != 2; i++) run();
	if (at(0) != MAGIC || at(4) != 2) {
		printf("  FAIL the program did not run to its write (%08x, %u)\n", at(0), at(4));
		return 1;
	}
	check(at(8) == NEW, "played: FLAG at power-on is %u, a new card", at(8));
	check(at(12) == 0, "played: FLAG after the write is %u", at(12));
	check(sector_is(0xA5), "played: the write is on the card");
	for (i = 0; i < 30; i++) run();
	check(at(20) == 0, "played: FLAG 30 frames on is %u, the same card", at(20));

	n = retro_serialize_size();
	s = malloc(n);
	if (!n || !s || !retro_serialize(s, n)) { printf("  FAIL no state\n"); return 1; }
	files(tag, statef, srmf);
	if (write_file(srmf, card, card_n) || write_file(statef, s, n)) { perror(statef); return 1; }
	return fails;
}

enum { SAME, CHANGED, OTHER_CORE, THIS_CORE };

/* The next start: the .srm, then the state `tag` kept. CHANGED: another
 * sector written on the card since the state was made. OTHER_CORE: the
 * state is from the other build, patched or not, which records the cards
 * differently or not at all */
static int resumed(const char *tag, int how)
{
	SYM(bool, retro_unserialize, (const void *, size_t))
	const char *what = how == CHANGED ? "resumed over a changed card"
	                 : how == OTHER_CORE ? "the other core's state, resumed in this one"
	                 : how == THIS_CORE ? "this core's state, resumed in the other one" : "resumed";
	char statef[600], srmf[600];
	size_t n, sn;
	uint8_t *srm, *s;
	uint32_t turns;
	int i;

	files(tag, statef, srmf);
	srm = read_file(srmf, &n);
	s = read_file(statef, &sn);
	if (!srm || n != card_n || !s) { printf("  FAIL no .srm or state from the run before\n"); return 1; }
	memcpy(card, srm, n);
	if (how == CHANGED) memset(card + (SECTOR - 1) * 128, 0x5A, 128);
	if (!retro_unserialize(s, sn)) { printf("  FAIL %s: the core refused the state\n", what); return 1; }
	check(at(4) == 2 && at(12) == 0, "%s: the core took the state (%zu bytes), main RAM is its",
	      what, sn);
	put32(ram + AT + 20, 0xFFFFFFFF);
	turns = at(16);
	for (i = 0; i < 30; i++) run();
	if (at(16) == turns || at(20) == 0xFFFFFFFF) {
		printf("  FAIL %s: the program did not run on\n", what);
		return 1;
	}
	if (how == CHANGED)
		check(at(20) == NEW, "%s: FLAG is %u, a new card, so the game reads it again",
		      what, at(20));
	else if (how == OTHER_CORE || how == THIS_CORE)
		/* One side has no record of the card: the core marks it new, as the
		 * buildbot core always did. Safe, and once only: the next state the
		 * patched core makes has the record */
		check(at(20) == NEW, "%s: FLAG is %u, a new card, as before the patch", what, at(20));
	else
		check(at(20) == 0, "%s: FLAG is %u%s", what, at(20),
		      at(20) == NEW ? ", a new card, though it is the card the state was made with"
		                    : ", the card the state was made with");
	check(sector_is(0xA5), "%s: the saved sector is on the card", what);
	return fails;
}

/* One run on `core`, in a process of its own, as a game is */
static int in_child(const char *core, int (*fn)(const char *, int), const char *tag, int how)
{
	pid_t p = fork();
	int st;

	if (p == 0) {
		setenv("CORE", core, 1);
		_exit(boot() ? 1 : fn(tag, how));
	}
	if (p < 0 || waitpid(p, &st, 0) != p) return 1;
	return !WIFEXITED(st) || WEXITSTATUS(st);
}

int main(int argc, char **argv)
{
	const char *core, *other;
	int bad = 0;

	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: %s core.so workdir [other.so]\n", argv[0]);
		return 2;
	}
	core = argv[1];
	other = argc == 4 ? argv[3] : NULL;
	setvbuf(stdout, NULL, _IOLBF, 0);
	snprintf(dir, sizeof dir, "%s", argv[2]);
	snprintf(sysdir, sizeof sysdir, "%s/system", dir);
	snprintf(savedir, sizeof savedir, "%s/saves", dir);
	snprintf(exe, sizeof exe, "%s/mcdtest.exe", dir);
	mkdir(sysdir, 0755);
	mkdir(savedir, 0755);
	if (write_exe(exe)) { perror(exe); return 2; }

	printf("the memory card across a resume, in %s\n", core);
	if (in_child(core, played, "core", 0)) {
		printf("FAILED before a state was made\n");
		return 1;
	}
	bad |= in_child(core, resumed, "core", SAME);
	bad |= in_child(core, resumed, "core", CHANGED);
	if (other) {
		printf("and with %s, the other build\n", other);
		if (in_child(other, played, "other", 0)) {
			printf("FAILED before the other core made a state\n");
			return 1;
		}
		/* Its state in this core, and this core's in it */
		bad |= in_child(core, resumed, "other", OTHER_CORE);
		bad |= in_child(other, resumed, "core", THIS_CORE);
	}
	printf(bad ? "FAILED\n" : "all ok\n");
	return bad;
}
