/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_INGAME_SCREEN_H
#define ZYPEROS_INGAME_SCREEN_H

/* The in-game menu (DESIGN.md 11.4): the menu over a paused game or a port,
 * its tabs (Game, Display, Controls, Achievements), the save slots' strip,
 * the Controls page, the achievement list and card, and a game's synopsis.
 * The screens are src/ingame_screen.c; the rows are src/game_menu.c's. */
#include "achtab.h"
#include "achtab_screen.h"
#include "app.h"
#include "controls.h"
#include "ctltab.h"
#include "ctltab_screen.h"
#include "menu_panel.h"
#include "panel.h"

typedef struct {
	int  count;
	int  index;
	char label[128];
} gm_disc_state;

typedef struct {
	int  tab;
	int  row[GT_TABS];
	bool scope_all;
} gm_mem_state;

/* The in-game menu's context. `resume` is what the epilogue turns on: whether
 * the player is going back to the game or the game is over. */
typedef struct {
	app         *a;
	SDL_Texture *bg;       /* the paused frame, as Diatom last wrote it */
	gm_bufs      bufs;
	menu_style   st;
	bool         resume;
	menu_tabs    tabs;     /* Game, Display, Controls; none over a port */
	int          seen;     /* the runner's cursor (menu_style.sel_out) */
	/* The Controls tab's rows' value colors and sound classes, which are
	 * more than gm_bufs holds (src/ctltab_screen.h) */
	unsigned      ct_vcol[CT_MAX_ROWS];
	unsigned char ct_sound[CT_MAX_ROWS];
	/* The Achievements tab's rows' values, colors and sounds
	 * (src/achtab_screen.h) */
	achtab_bufs   ach;
} gm_ctx;

/* What main.c uses of it; each is described where it is defined */
void paused_backdrop(app *a, SDL_Texture *bg, float k);
extern gm_disc_state g_gm_disc;
extern int g_shot_discs;
extern bool g_shot_undo;
extern const char *g_shot_disc_label;
extern gm_mem_state g_gm_mem;
extern SDL_Renderer *g_badge_r;
extern ctl_game g_port_ctl;
extern int shot_achtab;
extern bool g_menu_unasked;
void slot_when(char *out, size_t n, const struct tm *tm);
void slot_draw(app *a, const slot_view *sv, int sel);
void gm_gather(app *a, gm_ui *out);
void gm_mem_reset(void);
const char *gm_tab_name_i(int i);
int syn_layout(const char *text, int fixed,
               char (*lines)[CHV_WRAP_COLS], menu_row *rows,
               int *loop_at);
void synopsis_screen(app *a, const char *title, const char *text,
                     unsigned accent);
void cheevos_screen(app *a, SDL_Texture *bg, bool over_shelf);
bool ctl_head_fits(void *ctx, const char *head);
ctl_game game_controls_of(app *a);
int controls_page_rows(const ctl_game *g, int page, int width, ctl_buf *b,
                       const menu_row **out, int *loop_at);
int game_controls_width(app *a, const ctl_game *g);
int gm_width(app *a);
void gm_at_facts(at_facts *f);
at_source gm_at_source(void);
void gm_ctl_game(app *a, gm_ctx *c, ctlscr_game *g);
void gm_backdrop(app *a, void *ctx, float k);
/* The paused frame the Display list's picture draws (NULL: the stand-in),
 * and a shot's --gm-source */
void gm_preview_frame(SDL_Texture *bg);
extern int    shot_gm_src_w, shot_gm_src_h;
extern double shot_gm_aspect;
void game_menu(app *a);
int port_menu(app *a);

/* The in-game menu's option rows' choices by gm_row, for the state `u`
 * (menu.h's menu_choice): what gm_menu_build gives the runner */
const menu_choice *gm_choices(app *a, const gm_ui *u);

/* --rows-report: the in-game menu's Game and Display tabs (menu_rows_print) */
void gm_rows_report(app *a);

#endif
