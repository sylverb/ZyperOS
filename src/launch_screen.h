/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LAUNCH_SCREEN_H
#define ZYPEROS_LAUNCH_SCREEN_H

/* Launching: a game handed to Diatom, a port or a standalone program
 * handed to launch.sh, the resident emulator brought back, and Install
 * PortMaster's page (src/pminstall.h, DESIGN.md 11.32). The code is
 * src/launch_screen.c. */
#include "app.h"
#include "panel.h"
#include "pminstall.h"

/* Left by launch_port for the launcher launch.sh starts after the port, so it
 * opens on the game list the port was started from rather than on the systems
 * shelf - which is where a game in Diatom comes back to, since this process
 * never stops for one. Under /tmp, so a boot never sees one: a power-off in the
 * middle of a port starts on the shelf, as every boot does. */
#define PORT_RETURN  "/tmp/zyperos_port_return"

enum { PM_SHOT_RUNNING = 1, PM_SHOT_NONET = 2, PM_SHOT_NOPYTHON = 4, PM_SHOT_INSTALLED = 8,
       PM_SHOT_REPAIR = 16, PM_SHOT_FAILED = 32 };

/* What main.c uses of it; each is described where it is defined */
extern int g_pm_shot;
/* --pico8-page: a host shot of the page A opens on Splore while your PICO-8
 * is not on the card */
extern bool g_p8_shot;
void p8_page_shot(app *a);
extern int g_p8_said_shot;
void p8_said_shot(app *a, int k);
const pm_view *pm_view_read(const system_cfg *s, bool now);
SDL_Texture *pm_icon_tex(app *a);
void pm_install_screen(app *a, const system_cfg *s, bool repair, const char *failed,
                       bool crashed);
void pm_after_run(app *a);
void launch(app *a);

#endif
