/* SPDX-License-Identifier: GPL-3.0-only */
/* See dmodefit.h: diatom/src/scale.c's arithmetic, mode by mode */
#include "dmodefit.h"

#include <string.h>

static double shape(int src_w, int src_h, double aspect)
{
	return aspect > 0.0 ? aspect : (double)src_w / (double)src_h;
}

static dmode_rect centered(int w, int h, int surf_w, int surf_h)
{
	dmode_rect r = { (surf_w - w) / 2, (surf_h - h) / 2, w, h };
	return r;
}

dmode_rect dmode_fit(const char *name, int src_w, int src_h, double aspect,
                     int surf_w, int surf_h)
{
	double a;
	int f, fx, fy;

	if (!name || src_w <= 0 || src_h <= 0 || surf_w <= 0 || surf_h <= 0) {
		dmode_rect z = { 0, 0, 0, 0 };
		return z;
	}
	if (!strcmp(name, "native")) return centered(src_w, src_h, surf_w, surf_h);
	if (!strcmp(name, "integer")) {
		fx = surf_w / src_w;
		fy = surf_h / src_h;
		f = fx < fy ? fx : fy;
		if (f < 1) f = 1;
		return centered(src_w * f, src_h * f, surf_w, surf_h);
	}
	if (!strcmp(name, "integer-vertical")) {
		a = shape(src_w, src_h, aspect);
		for (f = surf_h / src_h; f > 1; f--)
			if ((int)((double)(src_h * f) * a + 0.5) <= surf_w) break;
		if (f < 1) f = 1;
		return centered((int)((double)(src_h * f) * a + 0.5), src_h * f, surf_w, surf_h);
	}
	if (!strcmp(name, "overscale")) {
		fx = (surf_w + src_w - 1) / src_w;
		fy = (surf_h + src_h - 1) / src_h;
		f = fx > fy ? fx : fy;
		if (f < 1) f = 1;
		return centered(src_w * f, src_h * f, surf_w, surf_h);
	}
	if (!strcmp(name, "aspect")) {
		a = shape(src_w, src_h, aspect);
		if ((double)surf_w / a <= (double)surf_h)
			return centered(surf_w, (int)((double)surf_w / a + 0.5), surf_w, surf_h);
		return centered((int)((double)surf_h * a + 0.5), surf_h, surf_w, surf_h);
	}
	if (!strcmp(name, "square")) {
		/* Square pixels at the full height; wider than the surface, the
		 * full width at the same factor */
		const int w = (2 * src_w * surf_h + src_h) / (2 * src_h);

		if (w <= surf_w) return centered(w, surf_h, surf_w, surf_h);
		return centered(surf_w, (2 * src_h * surf_w + src_w) / (2 * src_w), surf_w, surf_h);
	}
	if (!strcmp(name, "fill")) {
		a = shape(src_w, src_h, aspect);
		if ((double)surf_w / a >= (double)surf_h)
			return centered(surf_w, (int)((double)surf_w / a + 0.5), surf_w, surf_h);
		return centered((int)((double)surf_h * a + 0.5), surf_h, surf_w, surf_h);
	}
	/* stretch, and a word it does not know, as Diatom's default */
	return centered(surf_w, surf_h, surf_w, surf_h);
}
