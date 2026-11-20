/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_PORTLOAD_H
#define ZYPEROS_PORTLOAD_H

/* What the panel shows while a port loads (DESIGN.md 11.11), saved for
 * plastron's splash. Each is described in src/portload.c. */
#include "app.h"
#include "config.h"

#include <stdbool.h>

/* The screen for game `i` of the shelf, drawn; true when every line held
 * its contrast, false (said on stderr) fails --portload's shot */
bool port_loading_draw(app *a, const system_cfg *s, int i);
/* The line port_loading_draw writes: false the start's ("Starting. This can
 * take a minute."), true the one after a minute ("Still starting. Hold MENU
 * and START to go back.") */
extern bool g_port_loading_later;
/* The still for the game under the cursor, written for splash */
void port_loading_screen(app *a, const system_cfg *s);
/* The letters and the box splash draws the port's latest output with, for
 * the last screen port_loading_draw drew, written to `path`; false and no
 * file when it could not be written */
bool port_out_write(const char *path);

#endif
