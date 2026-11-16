/* SPDX-License-Identifier: GPL-3.0-only */
#include "focusfx.h"

#include "design.h"

void focusfx_reset(focusfx *f)
{
	*f = (focusfx){ .sys = -1, .idx = -1 };
}

bool focusfx_step(focusfx *f, int sys, int idx, bool generated, bool played, bool settled,
                  bool sharp_ready, uint32_t now, focusfx_out *out)
{
	bool landed = false;

	*out = (focusfx_out){ .sheen = -1 };
	/* Another card, or the row moving again: everything stops at once */
	if (sys != f->sys || idx != f->idx || !settled) {
		f->sys = sys;
		f->idx = idx;
		f->settled = false;
		f->sharp_from = 0;
	}
	if (!settled || sys < 0 || idx < 0) return false;
	if (!f->settled) {
		f->settled = true;
		f->since = now;
		landed = true;
	}
	if (generated) {
		uint32_t t = now - f->since;

		if (t < DS_MS_SHEEN) {
			out->sheen = ds_turn((float)t / DS_MS_SHEEN);
			out->moving = true;
			out->until = f->since + DS_MS_SHEEN;
		}
	}
	if (generated && played && sharp_ready) {
		uint32_t t;

		if (!f->sharp_from) f->sharp_from = now ? now : 1;
		t = now - f->sharp_from;
		out->sharp = t >= DS_MS_SHARPEN ? 1 : (float)t / DS_MS_SHARPEN;
		if (t < DS_MS_SHARPEN) {
			out->moving = true;
			if (f->sharp_from + DS_MS_SHARPEN > out->until) out->until = f->sharp_from + DS_MS_SHARPEN;
		}
	}
	return landed;
}
