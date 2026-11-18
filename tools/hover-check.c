/* SPDX-License-Identifier: GPL-3.0-only */
/* What an option list's cursor shows (src/ds/hover.h), its rules one case
 * each, against a list that records what it was told.
 *
 *   make check-hover
 *
 * A show once per choice the cursor comes to, never each pass; the kept one
 * put back with undo, at once; undo once on close only when something was
 * shown, so a choice A takes is stepped from the kept one; nothing at all
 * from a list with none; and ds_hover_showing true exactly while a choice
 * that is not kept is shown. */
#include "../src/ds/hover.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void ck(int cond, const char *what)
{
	if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

/* What the list was told, in order: a show as its choice, an undo as U */
typedef struct {
	char said[64];
	int n;
} tape;

static void show(void *ctx, int k)
{
	tape *t = ctx;

	if (t->n < (int)sizeof t->said - 1) t->said[t->n++] = (char)('0' + k);
	t->said[t->n] = '\0';
}

static void undo(void *ctx, int kept)
{
	tape *t = ctx;

	(void)kept;
	if (t->n < (int)sizeof t->said - 1) t->said[t->n++] = 'U';
	t->said[t->n] = '\0';
}

static const ds_hover LIST = { .show = show, .undo = undo };

static void shows_once_per_choice(void)
{
	tape t = { 0 };
	ds_hover_run r;

	ds_hover_begin(&r, &LIST, &t, 2, 2);
	ds_hover_at(&r, 2);                      /* opens on the kept one: nothing */
	ck(t.n == 0, "the kept one under the cursor as the list opens shows nothing");
	ds_hover_at(&r, 3);
	ds_hover_at(&r, 3);
	ds_hover_at(&r, 3);
	ck(!strcmp(t.said, "3"), "a choice is shown once, not on every pass");
	ds_hover_at(&r, 4);
	ck(!strcmp(t.said, "34"), "the next choice is shown when the cursor comes to it");
	ds_hover_at(&r, 2);
	ck(!strcmp(t.said, "34U"), "back on the kept one: undo, at once");
	ds_hover_at(&r, 2);
	ck(!strcmp(t.said, "34U"), "and only once");
	ds_hover_end(&r);
	ck(!strcmp(t.said, "34U"), "closed on the kept one: no undo, nothing was shown");
}

static void undo_on_close(void)
{
	tape t = { 0 };
	ds_hover_run r;

	ds_hover_begin(&r, &LIST, &t, 0, 0);
	ds_hover_at(&r, 1);
	ds_hover_end(&r);
	ck(!strcmp(t.said, "1U"), "closed on a shown choice: undo, before A's is applied");
	ds_hover_end(&r);
	ck(!strcmp(t.said, "1U"), "a spent run undoes nothing again");
}

static void starts_off_the_kept_one(void)
{
	tape t = { 0 };
	ds_hover_run r;

	ds_hover_begin(&r, &LIST, &t, 0, 3);
	ds_hover_at(&r, 3);
	ck(!strcmp(t.said, "3"), "a list opening with its cursor off the kept one shows it");
	ds_hover_end(&r);
}

static void without_a_show(void)
{
	static const ds_hover NONE = { .show = NULL };
	tape t = { 0 };
	ds_hover_run r;

	ds_hover_begin(&r, NULL, &t, 0, 0);
	ds_hover_at(&r, 1);
	ds_hover_end(&r);
	ds_hover_begin(&r, &NONE, &t, 0, 0);
	ds_hover_at(&r, 1);
	ds_hover_end(&r);
	ck(t.n == 0, "a list with no show, or no hover at all, is told nothing");
}

static void showing_while_shown(void)
{
	tape t = { 0 };
	ds_hover_run r;

	ck(!ds_hover_showing(), "nothing showing before a list opens");
	ds_hover_begin(&r, &LIST, &t, 0, 0);
	ds_hover_at(&r, 0);
	ck(!ds_hover_showing(), "nothing showing on the kept one");
	ds_hover_at(&r, 2);
	ds_hover_at(&r, 3);
	ck(ds_hover_showing(), "showing once a choice is shown, through the next");
	ds_hover_at(&r, 0);
	ck(!ds_hover_showing(), "not showing back on the kept one");
	ds_hover_at(&r, 1);
	ds_hover_end(&r);
	ck(!ds_hover_showing(), "not showing once the list has closed");
}

int main(void)
{
	shows_once_per_choice();
	undo_on_close();
	starts_off_the_kept_one();
	without_a_show();
	showing_while_shown();
	if (fails) { printf("\n%d hover check(s) failed\n", fails); return 1; }
	printf("ok: a list shows each choice once as its cursor comes to it, puts the kept one back with undo, "
	       "and undoes on close only what it showed\n");
	return 0;
}
