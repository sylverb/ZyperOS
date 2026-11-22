/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_P8FX_SEND_H
#define ZYPEROS_P8FX_SEND_H

/* The launcher's side of your PICO-8's Shader and Border (src/p8fx.h): the
 * look file written from the PICO-8 shelf's own rows, as the launcher hands
 * over to PICO-8 (launch_port) and as a row changes in the in-game menu over
 * it, where the shim in PICO-8's process reads it a few times a second. */

#include <stdbool.h>

#include "app.h"

/* Whether the Display mode system `sys` draws your PICO-8 in leaves room
 * round its 128x128 screen on the panel (src/p8draw.h's rect): where Blur
 * and Your Own are offered and drawn */
bool p8fx_room(const app *a, int sys);

/* System `sys`'s Shader, Strength and Border written to P8FX_FILE, Your
 * Own's picture made the panel's size into P8FX_BORDER_FILE first. A
 * picture that cannot be made is no border, said in the log. True when the
 * file is there. */
bool p8fx_send(app *a, int sys);

#endif
