/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LINEBREAK_H
#define ZYPEROS_LINEBREAK_H

/* A name too wide for its box, broken into lines, in the part of it that is
 * plain C: where the breaks go, given how wide a string is drawn. The
 * launcher measures with SDL_ttf (src/ui.c's ui_fit_text and
 * ui_balance_lines); tools/linebreak-check.c with a table of its own, so it
 * holds the rule with no font and no window.
 *
 * BALANCED, NOT GREEDY. A greedy break fills each line as far as it goes and
 * leaves what is over for the next, so a long name in a narrow column comes
 * out as full lines with one short word alone under them. A name is set here
 * the way a person would set it: in as few lines as it fits in, and of those
 * breaks the one whose widest line is narrowest, so the lines come out near
 * one length. The port loading screen breaks the game's name this way, in up
 * to three lines (src/main.c, port_loading_screen).
 *
 * FILLED, for a sentence set left in a list's place (lb_fill): the in-game
 * menu's Achievements tab, which says why a game has no list in a panel
 * every tab shares, as wide as the widest. Balanced there, its lines stopped
 * well short of the panel's right edge, a narrow column in a wide panel,
 * and a short first line alone ("ZyperOS kann" over a long one in German):
 * "The empty achievements text in the menu is not using the full menu's
 * width" (Tarik, testing 1.0.6). Filled, it reads as the panel's text, as
 * the same menu's synopsis and an achievement's description do (wrap_text). */

#include <stddef.h>

/* How wide `s` is drawn, in whatever units the box is given in. */
typedef int (*lb_width_fn)(void *ctx, const char *s);

/* Bytes in one line, its NUL included: a game's title (src/library.h's
 * LIB_NAME) whole. */
#define LB_LINE 256

/* The most lines a break is asked for: a cap over it is held to it. */
#define LB_MAX_LINES 8

/* `src` into `dst`, shortened with an ellipsis until it fits `maxw`: bytes
 * stepped back one at a time and then off any UTF-8 continuation, so a
 * character is never cut in half; nothing at all when not even the ellipsis
 * fits. src/ui.c's ui_fit_text is this, measured by SDL_ttf: one rule for
 * where the dots go, and one copy of it. */
void lb_fit(const char *src, char *dst, size_t dstn, int maxw,
            lb_width_fn width, void *ctx);

/* `text` in at most `cap` lines, none wider than `maxw`, into `lines`;
 * returns how many, 0 for a text with no words or a cap under 1.
 *
 *   One line when it fits.
 *   Otherwise the fewest lines it fits in, up to `cap`, broken between
 *   words, and of those breaks the one whose widest line is narrowest.
 *   A word wider than the box on its own is cut with an ellipsis (lb_fit)
 *   and stands as the cut word.
 *   A text that fits in no `cap` lines even so fills them in order, each with
 *   as many words as it takes, and the last is cut with an ellipsis.
 *
 * So never more than `cap` lines and never a line over `maxw`. Words are what
 * spaces separate: a run of spaces is one break, and spaces at either end are
 * dropped. The ">" of a way through the menus ("MENU > Achievements") goes
 * with the word before it: no line starts with one. Chinese, which has no spaces, breaks between its characters, but
 * not before a mark that closes (，。！) nor after one that opens (（「), and
 * its lines are joined with no space (src/linebreak.c, lb_piece). */
int lb_balance(const char *text, int maxw, int cap, lb_width_fn width,
               void *ctx, char lines[][LB_LINE]);

/* `text` in at most `cap` lines, none wider than `maxw`, filled: each line
 * with as many words as fit, the next word on the next line, and a text that
 * fits in no `cap` lines cut at the end of the last, as lb_balance cuts it.
 * The words, a word too wide for the box and Chinese's breaks as
 * lb_balance's; the fewest lines it fits in, as lb_balance's, but its lines
 * as full as they go, not near one length. */
int lb_fill(const char *text, int maxw, int cap, lb_width_fn width,
            void *ctx, char lines[][LB_LINE]);

/* Where an edge `at` units into `text` falls between its characters: into
 * *in the end of the last character that ends at or before `at`, 0 when none
 * does, and into *out the end of the first that ends at or after it, the
 * text's width when `at` is past its end. Both `at` when the edge is already
 * between two. Measured a whole character at a time, never inside the bytes
 * of one, as the text's first characters, so for text set left to right,
 * which is all the launcher sets; 0 and 0 for no text, and both `at` for a
 * text too long for the stack that cannot be copied.
 *
 * For src/ui.c's ui_text_marquee, which stops its window at these when a
 * name rests in it, so the window's end shows whole letters and not a sliver
 * of the next one. */
void lb_edges(const char *text, int at, lb_width_fn width, void *ctx,
              int *in, int *out);

#endif
