/* Engine B's extended backgrounds in a built DSperate core, frame for frame
 * against the core without mk/patches/dsperate-lr-ext-bg.patch
 * (mk/dsperate-extbg-check.sh, which mk/build-dsperate.sh runs on each core
 * it builds).
 *
 * The patch reads a rotated or scaled extended background (a bitmap, or a
 * 16-bit tile map) straight from host memory when every VRAM block it covers
 * is backed by exactly one bank and the blocks lie back to back, instead of a
 * per-block lookup for each pixel; otherwise the lookup, as before. Either
 * way each pixel must come out the same. This boots a ROM it writes itself,
 * whose ARM9 only runs commands this check leaves in main RAM (register and
 * VRAM writes, waits for VBlank; tools/dsperate-extbg-check.s), and shows on
 * engine B, in four bank layouts (bank C alone; banks H and I, not back to
 * back; C and H over each other, read together; C, with H as extended
 * palettes), eleven setups each: 8-bit bitmaps of every size (512x512 reaches
 * past the 128 KB view), direct colour wrapped and not, 16-bit tile maps of
 * every size with and without extended palettes, and two backgrounds at once
 * with a mosaic, all rotated and scaled. Each of the 96 frames is hashed
 * (both screens) and must equal GOLD, the hashes the release core made before
 * the patch (35d65a0d, 2026-11-16). With the patch made to read the wrong
 * byte, exactly the setups that can take the direct read change, and the
 * others stay the same, so both ways are run. Exit 0 when every frame is. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "libretro.h"

static char sysdir[512], savedir[512];
static int fails;

static void core_log(enum retro_log_level l, const char *fmt, ...)
{
	va_list a;

	if (l < RETRO_LOG_INFO) return;
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
	default: return false;   /* every option at its default: the recompiler on */
	}
}

/* Each frame the core shows, hashed (FNV-1a, both screens) */
static uint64_t frame_hash;
static void video(const void *d, unsigned w, unsigned h, size_t p)
{
	uint64_t x = 1469598103934665603ull;
	unsigned y, i;

	if (!d) return;
	for (y = 0; y < h; y++) {
		const uint8_t *row = (const uint8_t *)d + y * p;
		for (i = 0; i < w * 4; i++) x = (x ^ row[i]) * 1099511628211ull;
	}
	frame_hash = x;
}
static void audio1(int16_t l, int16_t r) { (void)l; (void)r; }
static size_t audio(const int16_t *d, size_t f) { (void)d; return f; }
static void poll_(void) {}
static int16_t input(unsigned a, unsigned b, unsigned c, unsigned d) { (void)a; (void)b; (void)c; (void)d; return 0; }

static void put32(uint8_t *p, uint32_t v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* The ARM9's whole program (tools/dsperate-extbg-check.s): it runs the
 * commands this check writes to main RAM at 0x02100000. */
static const uint32_t ARM9[] = {
	0xE3A00621, 0xE4901004, 0xE3510001, 0x0A000008, 0xE3510002, 0x0A00000A, 0xE3510003, 0x0A00000C,
	0xE3510004, 0x0A00000E, 0xE3510005, 0x0A000014, 0xEAFFFFFE, 0xE4902004, 0xE4903004, 0xE5823000,
	0xEAFFFFEF, 0xE4902004, 0xE4903004, 0xE1C230B0, 0xEAFFFFEB, 0xE4902004, 0xE4903004, 0xE5C23000,
	0xEAFFFFE7, 0xE59F2038, 0xE1D230B0, 0xE3130001, 0x1AFFFFFC, 0xE1D230B0, 0xE3130001, 0x0AFFFFFC,
	0xEAFFFFDF, 0xE4902004, 0xE4903004, 0xE4904004, 0xE4935004, 0xE4825004, 0xE2544004, 0xCAFFFFFB,
	0xEAFFFFD7, 0x04000004,
};

static int write_rom(const char *path)
{
	static uint8_t rom[0xA000];
	FILE *f;
	size_t i;

	memcpy(rom, "ZYPEROS EXB ZEXB01", 18);
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

/* ---- the command list ---- */
static uint8_t *ram;
static uint32_t cmd_at;
static void cmd(uint32_t w) { put32(ram + 0x100000 + cmd_at, w); cmd_at += 4; }
static void w32(uint32_t a, uint32_t v) { cmd(1); cmd(a); cmd(v); }
static void w16(uint32_t a, uint32_t v) { cmd(2); cmd(a); cmd(v); }
static void w8(uint32_t a, uint32_t v) { cmd(3); cmd(a); cmd(v); }
static void vbl(void) { cmd(4); }
static void copy(uint32_t dst, uint32_t src, uint32_t len) { cmd(5); cmd(dst); cmd(src); cmd(len); }

/* Engine B's registers */
#define DISPCNT_B 0x04001000u
#define BGCNT_B(n) (0x04001008u + 2 * (n))
#define BGP_B(n, k) (0x04001020u + 0x10 * ((n) - 2) + 2 * (k))   /* PA PB PC PD */
#define BGX_B(n) (0x04001028u + 0x10 * ((n) - 2))
#define BGY_B(n) (0x0400102Cu + 0x10 * ((n) - 2))
#define MOSAIC_B 0x0400104Cu
#define VRAMCNT(b) (0x04000240u + (b) + ((b) >= 7 ? 1 : 0))     /* A..G at 240-246, H 248, I 249 */

static uint32_t rng = 12345;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng; }

/* 0x02200000: 128 KB of VRAM bytes (a zero now and then: transparent), the
 * BG palette, 32 KB of extended palettes */
static void data(void)
{
	uint32_t i;
	for (i = 0; i < 0x20000; i++) {
		uint8_t b = (uint8_t)(rnd() >> 24);
		if ((b & 7) == 0) b = 0;
		ram[0x200000 + i] = b;
	}
	for (i = 0; i < 256; i++) { uint16_t c = (uint16_t)(rnd() >> 17); ram[0x220000 + 2 * i] = c; ram[0x220001 + 2 * i] = c >> 8; }
	for (i = 0; i < 0x8000; i++) ram[0x221000 + i] = (uint8_t)(rnd() >> 24);
}

/* The banks: 1 C as engine B's BG, 2 H and I (split, not back to back),
 * 3 C and H both at 0x06200000 (read ORed), 4 C as BG and H as BG
 * extended palettes */
static void banks(int layout)
{
	int b;
	for (b = 2; b <= 8; b++) w8(VRAMCNT(b), 0);
	if (layout == 2) {
		w8(VRAMCNT(7), 0x81); w8(VRAMCNT(8), 0x81);
		copy(0x06200000, 0x02200000, 0xC000);
	} else {
		w8(VRAMCNT(2), 0x84);
		copy(0x06200000, 0x02200000, 0x20000);
		if (layout == 3) { w8(VRAMCNT(7), 0x81); copy(0x06200000, 0x02210000, 0x8000); }
		if (layout == 4) { w8(VRAMCNT(7), 0x80); copy(0x06898000, 0x02221000, 0x8000); w8(VRAMCNT(7), 0x82); }
	}
	copy(0x05000400, 0x02220000, 0x200);
}

/* A rotated, scaled background on BG n: angle a (1/256 turns), scale s
 * (8.8), the screen's centre on texel (cx, cy) */
static void affine(int n, int a, int s, int32_t cx, int32_t cy)
{
	static const int16_t sin64[65] = {0,6,13,19,25,31,38,44,50,56,62,68,74,80,86,92,98,103,109,115,120,126,131,136,142,147,152,157,162,167,171,176,181,185,189,193,197,201,205,209,212,216,219,222,225,228,231,234,236,238,241,243,244,246,248,249,251,252,253,254,254,255,255,256,256};
	int q = a & 255, sn, cs;
	sn = q < 64 ? sin64[q] : q < 128 ? sin64[128 - q] : q < 192 ? -sin64[q - 128] : -sin64[256 - q];
	q = (a + 64) & 255;
	cs = q < 64 ? sin64[q] : q < 128 ? sin64[128 - q] : q < 192 ? -sin64[q - 128] : -sin64[256 - q];
	{
		const int32_t pa = cs * 256 / s, pb = -sn * 256 / s, pc = sn * 256 / s, pd = cs * 256 / s;
		w16(BGP_B(n, 0), (uint16_t)pa); w16(BGP_B(n, 1), (uint16_t)pb);
		w16(BGP_B(n, 2), (uint16_t)pc); w16(BGP_B(n, 3), (uint16_t)pd);
		w32(BGX_B(n), (uint32_t)(cx * 256 - (pa * 128 + pb * 96)) & 0x0FFFFFFF);
		w32(BGY_B(n), (uint32_t)(cy * 256 - (pc * 128 + pd * 96)) & 0x0FFFFFFF);
	}
}

static int frames, shown;
/* The setup's number at 0x02300000 for the frame's line, then two frames of it */
static void show(void) { w32(0x02300000, (uint32_t)++shown); vbl(); vbl(); frames += 2; }

static void program(void)
{
	int layout, sz, k;
	w32(0x04000304, 0x8203);   /* POWCNT1: both 2D engines, engine B below */
	for (layout = 1; layout <= 4; layout++) {
		banks(layout);
		/* 8-bit bitmaps, every size (512x512 reaches past the 128 KB view) */
		for (sz = 0; sz < 4; sz++) {
			w32(DISPCNT_B, 0x00010805);                           /* mode 5, BG3 */
			w16(BGCNT_B(3), (uint16_t)(0x80 | (sz << 14) | (k = sz & 1) << 13 | (sz == 1 ? 1 : 0) << 8));
			affine(3, 23 + 40 * sz, 256 - 30 * sz, (sz >= 2 ? 256 : 64 << sz) + (int32_t)(rnd() & 31) - 16, (sz == 3 ? 256 : 64 << (sz ? 1 : 0)) + (int32_t)(rnd() & 31) - 16);
			show();
		}
		/* direct colour, wrapped and not */
		for (k = 0; k < 2; k++) {
			w32(DISPCNT_B, 0x00010805);
			w16(BGCNT_B(3), (uint16_t)(0x84 | (1 << 14) | k << 13));
			affine(3, 90 + 77 * k, 180 + 120 * k, 128 + 40 * k, 100 - 30 * k);
			show();
		}
		/* 16-bit tile maps, every size, extended palettes on and off */
		for (sz = 0; sz < 4; sz++) {
			w32(DISPCNT_B, 0x00010805 | (sz & 1 ? 0x40000000u : 0));
			w16(BGCNT_B(3), (uint16_t)((sz << 14) | (sz & 2) << 12 | (2 << 2) | (sz << 8)));
			affine(3, 200 - 31 * sz, 256 + 40 * sz, (64 << sz) + (int32_t)(rnd() & 63) - 32, (64 << sz) - 20);
			show();
		}
		/* both extended BGs, a mosaic on one */
		w32(DISPCNT_B, 0x00010C05);
		w16(BGCNT_B(2), (uint16_t)(0x80 | (1 << 14) | 0x40 | (2 << 8)));
		w16(BGCNT_B(3), (uint16_t)((1 << 14) | (1 << 13) | (1 << 8)));
		w16(MOSAIC_B, 0x0033);
		affine(2, 51, 200, 128, 128);
		affine(3, 170, 300, 120, 140);
		show();
		w16(MOSAIC_B, 0);
		w16(BGCNT_B(2), 0);
	}
	cmd(0);
}

/* The 96 frames' hashes from the release core before the patch (35d65a0d) */
#define FRAMES 96
static const uint64_t GOLD[FRAMES] = {
	0x2c68c528ebfb0383ull, 0x2c68c528ebfb0383ull, 0x9108ded3ab02564full, 0x5c09cc612ccc6e4full,
	0x96c3ce8507434d63ull, 0x96c3ce8507434d63ull, 0x75df093d13059319ull, 0x75df093d13059319ull,
	0xa3a3466362f825bfull, 0xa3a3466362f825bfull, 0x6c238e774147fd2aull, 0x6c238e774147fd2aull,
	0x6dfad6865ae8d194ull, 0x6dfad6865ae8d194ull, 0x89728cab1118a6deull, 0x89728cab1118a6deull,
	0x1acbedfea46c3568ull, 0x1acbedfea46c3568ull, 0x87e7519f1575c9e4ull, 0x87e7519f1575c9e4ull,
	0xa75b9f5c9dbb4333ull, 0xa75b9f5c9dbb4333ull, 0xe0c0988e1877956full, 0xe0c0988e1877956full,
	0x7573359947397d07ull, 0x7b2d61d0d14cd66dull, 0x4bc4cf70a12bd7afull, 0x4bc4cf70a12bd7afull,
	0x0697e8d73511e8caull, 0x0697e8d73511e8caull, 0xf16ae67abee7efb4ull, 0xf16ae67abee7efb4ull,
	0x7fe37f6955dd62dcull, 0x7fe37f6955dd62dcull, 0xb81de39e8b2b7ff7ull, 0xb81de39e8b2b7ff7ull,
	0x43039f6f76acf5c3ull, 0x43039f6f76acf5c3ull, 0x166a09110d9804d3ull, 0x166a09110d9804d3ull,
	0x254b9e97e87f72d4ull, 0x254b9e97e87f72d4ull, 0x5981a79e4c3cf67bull, 0x5981a79e4c3cf67bull,
	0x9c59c197bd7983dfull, 0x9c59c197bd7983dfull, 0x68c343979f5e0b42ull, 0x68c343979f5e0b42ull,
	0xadb1379ae7e7f5afull, 0x6ef255b61339a4d7ull, 0xd35b5177d3235529ull, 0xd35b5177d3235529ull,
	0x50a5f3c4e2adc15cull, 0x50a5f3c4e2adc15cull, 0x21b4bee07a555adbull, 0x21b4bee07a555adbull,
	0x3ec2a83e1b5d4216ull, 0x3ec2a83e1b5d4216ull, 0x845c56c8b7302236ull, 0x845c56c8b7302236ull,
	0xca68e01b45a3c937ull, 0xca68e01b45a3c937ull, 0xd608df7ea204cc7full, 0xd608df7ea204cc7full,
	0xcf64ec2399f42825ull, 0xcf64ec2399f42825ull, 0x634011cdb2bf69e7ull, 0x634011cdb2bf69e7ull,
	0xea6541f361f63bbdull, 0xea6541f361f63bbdull, 0x763d8b874d736d2full, 0x68c343979f5e0b42ull,
	0x21dbc70d5c23b674ull, 0xaed722e7f7ad768full, 0xc57cd9eab7747401ull, 0xc57cd9eab7747401ull,
	0xa7c5a90d36b14459ull, 0xa7c5a90d36b14459ull, 0x006d485b3161aedfull, 0x006d485b3161aedfull,
	0x6c238e774147fd2aull, 0x6c238e774147fd2aull, 0x6dfad6865ae8d194ull, 0x6dfad6865ae8d194ull,
	0x8ca4e9477cc89e9aull, 0x8ca4e9477cc89e9aull, 0xdb046fb21bb2a109ull, 0xdb046fb21bb2a109ull,
	0x89bb5fefef483d81ull, 0x89bb5fefef483d81ull, 0x8163caa6b2c2113cull, 0x8163caa6b2c2113cull,
	0xe0c0988e1877956full, 0xe0c0988e1877956full, 0x68c343979f5e0b42ull, 0x68c343979f5e0b42ull,
};

int main(int argc, char **argv)
{
	void *so;
	void (*set_env)(retro_environment_t);
	void (*set_video)(retro_video_refresh_t), (*set_audio1)(retro_audio_sample_t);
	void (*set_audio)(retro_audio_sample_batch_t), (*set_poll)(retro_input_poll_t);
	void (*set_input)(retro_input_state_t), (*init)(void), (*run)(void);
	bool (*load)(const struct retro_game_info *);
	void *(*mem)(unsigned);
	char rom[600];
	int f;

	if (argc < 3) { fprintf(stderr, "usage: %s core.so dir\n", argv[0]); return 2; }
	snprintf(sysdir, sizeof sysdir, "%s/sys", argv[2]);
	snprintf(savedir, sizeof savedir, "%s/save", argv[2]);
	snprintf(rom, sizeof rom, "%s/extbg.nds", argv[2]);
	{ char c[1200]; snprintf(c, sizeof c, "mkdir -p '%s' '%s'", sysdir, savedir); if (system(c)) return 2; }
	if (write_rom(rom)) { perror(rom); return 2; }
	if (!(so = dlopen(argv[1], RTLD_NOW))) { fprintf(stderr, "%s\n", dlerror()); return 2; }
	set_env = dlsym(so, "retro_set_environment"); set_video = dlsym(so, "retro_set_video_refresh");
	set_audio1 = dlsym(so, "retro_set_audio_sample"); set_audio = dlsym(so, "retro_set_audio_sample_batch");
	set_poll = dlsym(so, "retro_set_input_poll"); set_input = dlsym(so, "retro_set_input_state");
	init = dlsym(so, "retro_init"); run = dlsym(so, "retro_run"); load = dlsym(so, "retro_load_game");
	mem = dlsym(so, "retro_get_memory_data");
	if (!set_env || !run || !load || !mem) { fprintf(stderr, "not a libretro core\n"); return 2; }
	set_env(env); set_video(video); set_audio1(audio1); set_audio(audio); set_poll(poll_); set_input(input);
	init();
	{ struct retro_game_info g = { .path = rom }; if (!load(&g)) { fprintf(stderr, "no load\n"); return 2; } }
	ram = mem(RETRO_MEMORY_SYSTEM_RAM);
	if (!ram) return 2;
	data();
	program();
	if (frames + 8 != FRAMES) { fprintf(stderr, "the program changed: %d frames, GOLD has %d\n", frames + 8, FRAMES); return 2; }
	printf("engine B's extended backgrounds, %d frames in four bank layouts, against the core before the patch\n", FRAMES);
	for (f = 0; f < FRAMES; f++) {
		/* The setup a frame shows is the one in place when it starts */
		const unsigned setup = ram[0x300000] | ram[0x300001] << 8;

		run();
		if (frame_hash != GOLD[f]) {
			printf("  FAIL frame %d (bank layout %u, setup %u): %016llx, expected %016llx\n", f,
			       setup ? (setup - 1) / 11 + 1 : 0, setup ? (setup - 1) % 11 + 1 : 0,
			       (unsigned long long)frame_hash, (unsigned long long)GOLD[f]);
			fails++;
		}
	}
	printf(fails ? "%d of %d frames differ\n" : "all ok: every frame as before\n", fails, FRAMES);
	return fails ? 1 : 0;
}
