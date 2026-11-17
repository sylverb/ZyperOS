/* SPDX-License-Identifier: GPL-3.0-only */
/* A page's option lists swept through what their cursors show
 * (src/hover_sweep.h) */
#include "hover_sweep.h"

#include "db.h"
#include "ds/hover.h"

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
	if (!build) return;
	pr->n = build(a, ctx, rows, SWEEP_ROWS, &ch);
	if (pr->n > SWEEP_ROWS) pr->n = SWEEP_ROWS;
	for (i = 0; i < pr->n; i++)
		snprintf(pr->value[i], sizeof pr->value[i], "%s\t%s", rows[i].label ? rows[i].label : "",
		         rows[i].value == MENU_TICK ? "(tick)" : rows[i].value ? rows[i].value : "");
}

static long written(void)
{
	return db_changes(db_dev()) + db_changes(db_lib());
}

/* Each choice shown in turn, as a cursor going down the list would, then
 * back on the kept one, then the list closed: the shows it made */
static int sweep(app *a, const ds_hover *h, int kept, int n)
{
	ds_hover_run r;
	unsigned now = 0;
	int k, shown = 0;

	ds_hover_begin(&r, h, a, kept, kept >= 0 ? kept : 0, now);
	for (k = 0; k < n; k++) {
		/* past any rest: the cursor stops on each */
		now += h->rest_ms + 1000;
		ds_hover_at(&r, k, now);
		ds_hover_at(&r, k, now + h->rest_ms + 1);
		if (r.shown == k) shown++;
	}
	if (kept >= 0) ds_hover_at(&r, kept, now + 1);
	ds_hover_end(&r);
	return shown;
}

void hover_sweep_list(app *a, const char *page, const char *label, const ds_hover *h,
                      int kept, int n, hover_sweep_build build, void *ctx)
{
	page_read before, after;
	long w0;
	int shown;

	if (!h || !h->show || n <= 0) return;
	page_take(a, build, ctx, &before);
	w0 = written();
	shown = sweep(a, h, kept, n);
	page_take(a, build, ctx, &after);
	printf("hover: %s\t%s\t%d\t%ld\t%s\n", page, label ? label : "", shown, written() - w0,
	       before.n == after.n && !memcmp(before.value, after.value, sizeof before.value) ? "same" : "changed");
	fflush(stdout);
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
	for (i = 0; i < n; i++)
		if (c[i].n > 2 && c[i].hover)
			hover_sweep_list(a, page, label[i], c[i].hover, c[i].none ? -1 : c[i].cur, c[i].n, build, ctx);
}
