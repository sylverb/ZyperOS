/* SPDX-License-Identifier: GPL-3.0-only */
/* What an option list's cursor shows (src/ds/hover.h) */
#include "ds/hover.h"

#include <stddef.h>

/* Runs showing a choice now (ds_hover_showing) */
static int g_showing;

void ds_hover_begin(ds_hover_run *r, const ds_hover *h, void *ctx, int kept, int at)
{
	r->h = h;
	r->ctx = ctx;
	r->kept = kept;
	r->at = at;
	r->shown = -1;
}

void ds_hover_at(ds_hover_run *r, int k)
{
	if (!r->h || !r->h->show) return;
	r->at = k;
	if (k == r->kept || k < 0) {
		/* Back on the kept one: what is kept, at once */
		if (r->shown >= 0) {
			if (r->h->undo) r->h->undo(r->ctx, r->kept);
			r->shown = -1;
			g_showing--;
		}
		return;
	}
	if (k == r->shown) return;
	r->h->show(r->ctx, k);
	if (r->shown < 0) g_showing++;
	r->shown = k;
}

void ds_hover_end(ds_hover_run *r)
{
	if (r->h && r->shown >= 0) {
		if (r->h->undo) r->h->undo(r->ctx, r->kept);
		g_showing--;
	}
	r->shown = -1;
	r->h = NULL;
}

bool ds_hover_showing(void)
{
	return g_showing > 0;
}
