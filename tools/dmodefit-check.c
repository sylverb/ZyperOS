/* SPDX-License-Identifier: GPL-3.0-only */
/* The launcher's display-mode arithmetic (src/dmodefit.h) against Diatom's
 * own (diatom/src/scale.c): every mode Diatom offers, over the sizes its
 * cores report (the Game Boy's to the SNES's hires, two screens stacked),
 * square pixels and the shapes cores ask for, on the surfaces ZyperOS's
 * handhelds have. A mode the two put in different places is a preview that
 * lies about it. Then Square Pixels' sizes, which both must reach. */
#include <stdio.h>
#include <string.h>

#include "../src/dmodefit.h"
#include "diatom.h"

/* Square Pixels (#346): the core's pixels square whatever shape it asks for,
 * the picture the screen's full height, its width in proportion, centred;
 * wider than the screen, the width fills it instead. The sizes the issue
 * names (NES and SNES 8:7, Game Gear 10:9, PC Engine 16:15) on both
 * handhelds, and two that are wider than the screen, from Diatom and from
 * the launcher alike: a shape the core asks for changes nothing. The cores'
 * own sizes and shapes are diatom/docs/reference/core-facts.md's (fceumm's
 * 256x224 asks for 1.3061, genesis_plus_gx's Game Gear for 4:3). */
static int square_sizes(void)
{
	static const struct { const char *what; int src_w, src_h; double aspect; int surf_w, surf_h, w, h, x, y; } CASES[] = {
		{ "NES 256x224 (8:7)",         256, 224, 1.3061,    1024, 768,  878, 768,  73,  0 },
		{ "NES 256x224 (8:7)",         256, 224, 1.3061,     640, 480,  549, 480,  45,  0 },
		{ "SNES 256x224 (8:7)",        256, 224, 4.0 / 3.0, 1024, 768,  878, 768,  73,  0 },
		{ "SNES 256x224 (8:7)",        256, 224, 4.0 / 3.0,  640, 480,  549, 480,  45,  0 },
		{ "NES uncut 256x240 (16:15)", 256, 240, 4.0 / 3.0, 1024, 768,  819, 768, 102,  0 },
		{ "NES uncut 256x240 (16:15)", 256, 240, 4.0 / 3.0,  640, 480,  512, 480,  64,  0 },
		{ "Game Gear 160x144 (10:9)",  160, 144, 4.0 / 3.0, 1024, 768,  853, 768,  85,  0 },
		{ "Game Gear 160x144 (10:9)",  160, 144, 4.0 / 3.0,  640, 480,  533, 480,  53,  0 },
		{ "PC Engine 256x240 (16:15)", 256, 240, 1.2150,    1024, 768,  819, 768, 102,  0 },
		{ "PC Engine 256x240 (16:15)", 256, 240, 1.2150,     640, 480,  512, 480,  64,  0 },
		{ "GBA 240x160, wider",        240, 160, 0.0,       1024, 768, 1024, 683,   0, 42 },
		{ "GBA 240x160, wider",        240, 160, 0.0,        640, 480,  640, 427,   0, 26 },
		{ "PSP 480x272, wider",        480, 272, 0.0,       1024, 768, 1024, 580,   0, 94 },
	};
	int i, m, bad = 0;

	for (m = 0; m < diatom_mode_count && strcmp(diatom_modes[m].name, "square"); m++) ;
	if (m == diatom_mode_count) {
		printf("  FAIL Diatom has no display mode \"square\" (diatom/src/scale.c)\n");
		return 1;
	}
	for (i = 0; i < (int)(sizeof CASES / sizeof CASES[0]); i++) {
		diatom_rect d = diatom_scale_rect(diatom_modes[m].mode, CASES[i].src_w, CASES[i].src_h,
		                                  CASES[i].aspect, CASES[i].surf_w, CASES[i].surf_h);
		dmode_rect z = dmode_fit("square", CASES[i].src_w, CASES[i].src_h, CASES[i].aspect,
		                         CASES[i].surf_w, CASES[i].surf_h);

		if (d.w != CASES[i].w || d.h != CASES[i].h || d.x != CASES[i].x || d.y != CASES[i].y) {
			printf("  FAIL square %s on %dx%d: Diatom %dx%d+%d+%d, want %dx%d+%d+%d\n", CASES[i].what,
			       CASES[i].surf_w, CASES[i].surf_h, d.w, d.h, d.x, d.y,
			       CASES[i].w, CASES[i].h, CASES[i].x, CASES[i].y);
			bad++;
		}
		if (z.w != CASES[i].w || z.h != CASES[i].h || z.x != CASES[i].x || z.y != CASES[i].y) {
			printf("  FAIL square %s on %dx%d: launcher %dx%d+%d+%d, want %dx%d+%d+%d\n", CASES[i].what,
			       CASES[i].surf_w, CASES[i].surf_h, z.w, z.h, z.x, z.y,
			       CASES[i].w, CASES[i].h, CASES[i].x, CASES[i].y);
			bad++;
		}
	}
	return bad;
}

int main(void)
{
	static const int SRC[][2] = {
		{ 160, 144 }, { 240, 160 }, { 256, 192 }, { 256, 224 }, { 256, 240 }, { 256, 243 },
		{ 320, 224 }, { 320, 240 }, { 368, 240 }, { 512, 448 }, { 640, 480 }, { 256, 384 },
		{ 480, 272 }, { 304, 224 }, { 1024, 768 }, { 1280, 720 }, { 1, 1 },
	};
	static const double ASPECT[] = { 0.0, 4.0 / 3.0, 8.0 / 7.0, 10.0 / 9.0, 16.0 / 9.0, 3.0 / 2.0 };
	static const int SURF[][2] = { { 640, 480 }, { 1024, 768 }, { 720, 720 }, { 1280, 720 } };
	int m, s, a, f, n = 0, bad = 0;

	for (m = 0; m < diatom_mode_count; m++)
		for (s = 0; s < (int)(sizeof SRC / sizeof SRC[0]); s++)
			for (a = 0; a < (int)(sizeof ASPECT / sizeof ASPECT[0]); a++)
				for (f = 0; f < (int)(sizeof SURF / sizeof SURF[0]); f++) {
					diatom_rect d = diatom_scale_rect(diatom_modes[m].mode, SRC[s][0], SRC[s][1],
					                                  ASPECT[a], SURF[f][0], SURF[f][1]);
					dmode_rect z = dmode_fit(diatom_modes[m].name, SRC[s][0], SRC[s][1],
					                         ASPECT[a], SURF[f][0], SURF[f][1]);

					n++;
					if (d.x != z.x || d.y != z.y || d.w != z.w || d.h != z.h) {
						if (bad < 10)
							printf("  FAIL %s %dx%d aspect %.3f on %dx%d: Diatom %dx%d+%d+%d, "
							       "launcher %dx%d+%d+%d\n", diatom_modes[m].name, SRC[s][0],
							       SRC[s][1], ASPECT[a], SURF[f][0], SURF[f][1], d.w, d.h, d.x, d.y,
							       z.w, z.h, z.x, z.y);
						bad++;
					}
				}
	if (square_sizes()) { printf("\nSquare Pixels' sizes FAILED\n"); return 1; }
	if (bad) { printf("\n%d of %d FAILED\n", bad, n); return 1; }
	printf("ok: the Display list's previews put each picture where Diatom does (%d cases, %d modes)\n",
	       n, diatom_mode_count);
	return 0;
}
