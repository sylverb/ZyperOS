/* SPDX-License-Identifier: GPL-3.0-only */
/* A page's option lists swept through what their cursors show
 * (src/hover_sweep.h) */
#include "hover_sweep.h"

#include "db.h"
#include "ds/hover.h"
#include "panel.h"

#include <SDL.h>
#include <stdio.h>
#include <string.h>

#define SWEEP_ROWS 64

typedef struct {
	int n;
	char value[SWEEP_ROWS][96];
} page_read;

static void page_take(app *a, hover_sweep_build build, void *ctx, page_read *pr)
{
	menu_row rows[SWEEP_ROWS];
	const menu_choice *ch = NULL;
	int i;

	memset(pr, 0, sizeof *pr);
	pr->n = build(a, ctx, rows, SWEEP_ROWS, &ch);
	if (pr->n > SWEEP_ROWS) pr->n = SWEEP_ROWS;
	for (i = 0; i < pr->n; i++)
		snprintf(pr->value[i], sizeof pr->value[i], "%s\t%s", rows[i].label ? rows[i].label : "",
		         ROW_IS_NOTE(rows[i]) ? "(note)" : rows[i].value == MENU_TICK ? "(tick)"
		         : rows[i].value ? rows[i].value : "");
}

static long written(void)
{
	return db_changes(db_dev()) + db_changes(db_lib());
}

static double ms_since(Uint64 t0)
{
	return (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

/* Each choice shown in turn, as a cursor going down the list would, three
 * frames of the shelf behind it drawn after each, then back on the kept
 * one, then the list closed: the shows it made, and the slowest show and
 * frame */
static int sweep(app *a, const ds_hover *h, int kept, int n, double *show_ms, double *frame_ms)
{
	ds_hover_run r;
	int k, f, shown = 0;

	*show_ms = *frame_ms = 0;
	ds_hover_begin(&r, h, a, kept, kept >= 0 ? kept : 0);
	for (k = 0; k < n; k++) {
		Uint64 t0 = SDL_GetPerformanceCounter();
		double ms;

		ds_hover_at(&r, k);
		ms = ms_since(t0);
		if (r.shown == k) {
			shown++;
			if (ms > *show_ms) *show_ms = ms;
		}
		for (f = 0; f < 3; f++) {
			t0 = SDL_GetPerformanceCounter();
			draw_shelf(a);
			ms = ms_since(t0);
			if (ms > *frame_ms) *frame_ms = ms;
		}
	}
	if (kept >= 0) ds_hover_at(&r, kept);
	ds_hover_end(&r);
	return shown;
}

void hover_sweep_page(app *a, const char *page, hover_sweep_build build, void *ctx)
{
	menu_row rows[SWEEP_ROWS];
	const menu_choice *ch = NULL;
	char label[SWEEP_ROWS][96];
	menu_choice c[SWEEP_ROWS];
	int n, i;

	n = build(a, ctx, rows, SWEEP_ROWS, &ch);
	if (n > SWEEP_ROWS) n = SWEEP_ROWS;
	/* Copied first: the build writes its tables again each time */
	for (i = 0; i < n; i++) {
		const menu_choice *ci = menu_choice_at(ch, n, i);

		snprintf(label[i], sizeof label[i], "%s", rows[i].label ? rows[i].label : "");
		c[i] = ci ? *ci : (menu_choice){ 0 };
	}
	for (i = 0; i < n; i++) {
		page_read before, after;
		double show_ms, frame_ms;
		long w0;
		int shown;

		if (c[i].n <= 2 || !c[i].hover || !c[i].hover->show) continue;
		page_take(a, build, ctx, &before);
		w0 = written();
		shown = sweep(a, c[i].hover, c[i].none ? -1 : c[i].cur, c[i].n, &show_ms, &frame_ms);
		page_take(a, build, ctx, &after);
		printf("hover: %s\t%s\t%d\t%ld\t%s\t%.2f\t%.2f\n", page, label[i], shown, written() - w0,
		       before.n == after.n && !memcmp(before.value, after.value, sizeof before.value) ? "same" : "changed",
		       show_ms, frame_ms);
		fflush(stdout);
	}
}
