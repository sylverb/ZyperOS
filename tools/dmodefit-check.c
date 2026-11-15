/* SPDX-License-Identifier: GPL-3.0-only */
/* The launcher's display-mode arithmetic (src/dmodefit.h) against Diatom's
 * own (diatom/src/scale.c): every mode Diatom offers, over the sizes its
 * cores report (the Game Boy's to the SNES's hires, two screens stacked),
 * square pixels and the shapes cores ask for, on the surfaces ZyperOS's
 * handhelds have. A mode the two put in different places is a preview that
 * lies about it. */
#include <stdio.h>
#include <string.h>

#include "../src/dmodefit.h"
#include "diatom.h"

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
	if (bad) { printf("\n%d of %d FAILED\n", bad, n); return 1; }
	printf("ok: the Display list's previews put each picture where Diatom does (%d cases, %d modes)\n",
	       n, diatom_mode_count);
	return 0;
}
