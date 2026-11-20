/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_DESK_H
#define ZYPEROS_DESK_H

/* Desk mode: the host build (make native, build-native/zyperos) in a window
 * as a handheld, for working on the UI without one (docs/building.md, "Work on
 * the UI without a device"). Host only: mk/native.mk builds this file and
 * defines ZYPEROS_DESK; a device build has neither.
 *
 * The window is the handheld's panel, the Pixel 2's 640x480 or the Brick's
 * 1024x768 (--device), at a zoom: ZyperOS draws its 1024x768 screen into a
 * texture as on the Pixel 2, the screen goes down to the panel's pixels and
 * up to the window. Under it, the handheld's buttons, drawn, each with its key:
 * clicked and held with the mouse, and lit while held from anywhere. The
 * keyboard and a game controller are the buttons too, with one key map for
 * desk mode and the Simulator (docs/simulator.md). Frames go up at the
 * panel's 60 a second (ZYPEROS_DESK_HZ, 0 for the display's own), and a
 * control socket (tools/desk) presses buttons, shoots the panel and times a
 * press to its frame.
 *
 * A window run is desk mode; a headless one (ZYPEROS_HEADLESS: the shots,
 * the checks) never is. */

#include <SDL.h>
#include <stdbool.h>

#include "../platform.h"

/* Whether this run is desk mode: a window, not a shot */
bool desk_on(void);

/* The window and its renderer, and the 1024x768 texture ZyperOS draws its
 * screen into, the render target when this returns */
bool desk_video_init(SDL_Window **win, SDL_Renderer **ren);
void desk_video_quit(void);

/* Where ZyperOS's screen is drawn: the texture, in desk mode (else NULL, the
 * window) */
SDL_Texture *desk_screen(void);

/* The screen to the window: down to the panel, up to the zoom, the buttons
 * under it, presented (at the pace asked for). The render target is the
 * screen again after. */
void desk_present(SDL_Renderer *ren);

/* One event from plat_input_poll: keys, the controller, the mouse on the
 * buttons, the window's size, the control socket. True when it was desk
 * mode's (the caller does nothing more with it). */
bool desk_event(const SDL_Event *e, in_state *st);

/* Once a poll, before the events: the wheel's taps let go */
void desk_poll_begin(in_state *st);

/* The shared key map (docs/simulator.md): the button a key is, by its
 * place on the keyboard, or IN_NONE */
in_button desk_key_button(SDL_Scancode sc);

/* The wait for input when there is no input file to poll: until an event
 * comes or ms go by, so a key wakes the launcher at once */
void desk_wait(unsigned ms);

/* The Screen Colors last put on the panel (plat_screen_colors), and whether
 * kept or a list's preview, for `state`: the host has no gamma table, so
 * this is where a script reads what the display's would hold */
void desk_note_colors(const screen_colors *c, bool keep);

/* A game, from the launcher's device_display_release to its
 * device_display_take: Diatom's frames from the desk frame file
 * (ZYPEROS_DESK_FB, diatom/include/diatom_deskfb.h) in the window, the keys,
 * the mouse and the controller to Diatom through it. desk_game_wait is
 * device_game_wait: the newest frame shown, the events taken, and the ms the
 * launcher may wait for Diatom's next line. */
void     desk_game_begin(void);
void     desk_game_end(void);
unsigned desk_game_wait(unsigned ms);

/* plat_input_flush's: the queued presses dropped, as on a handheld, and the
 * rest kept for the next poll (the socket's commands, the window closed, a
 * controller plugged in) */
void desk_flush(void);

/* POWER in a game: 1 down, 0 up, -1 none left (device_power_pressed) */
int desk_power_edge(void);

/* What the shelf showed in its last frame, for the control socket's `state`
 * (tools/desk): "systems" or "games", the cursor there and its name, so a
 * script can press a button and read where it landed */
void desk_note_focus(const char *screen, int cursor, const char *name);

#endif
