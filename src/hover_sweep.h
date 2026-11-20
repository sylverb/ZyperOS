/* SPDX-License-Identifier: GPL-3.0-only */
/* For check-hover-sweep (--hover-sweep, tools/hover-sweep-check.py): a
 * page's option lists whose cursors show a choice, each choice shown and the
 * kept one put back as its list would (src/ds/hover.h), proving a show
 * writes nothing and leaves the page as it found it, and what a show costs.
 * A line each:
 *
 *   hover: <page>\t<row>\t<shown>\t<db rows written>\t<same|changed>\t<show ms>\t<frame ms>
 *
 * "same": the page built again after reads the same, every row's value;
 * <show ms> the slowest show, <frame ms> the slowest of the three shelf
 * frames drawn after each show (the screen behind the list repainted in
 * the shown colors), on this machine. */
#ifndef ZYPEROS_HOVER_SWEEP_H
#define ZYPEROS_HOVER_SWEEP_H

#include "app.h"
#include "menu.h"

/* A page's rows and choices, built as its screen builds them: the rows into
 * `rows` (up to `max`), its choices by place into *choices; how many rows */
typedef int (*hover_sweep_build)(app *a, void *ctx, menu_row *rows, int max, const menu_choice **choices);

/* Every row of the page whose choices' hover shows */
void hover_sweep_page(app *a, const char *page, hover_sweep_build build, void *ctx);

#endif
