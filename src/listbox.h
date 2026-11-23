/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_LISTBOX_H
#define ZYPEROS_LISTBOX_H

/* The List view's boxes (DESIGN.md 11.39, src/gamelist.h), laid out by
 * rules (src/ds/flex.h). The heading and the foot are the cover row's
 * (games_frame), so turning View over moves nothing but the middle; in the
 * middle, from the page's margins, the list's column and the focused
 * entry's media, LB_GAP apart.
 *
 * The media's box is as tall as the rows in sight (448) and square on the
 * 4:3 canvas; a wider canvas gives it the width past 4:3's, up to a 4:3
 * snap's at that height (600 on the Smart Pro's 1368), and the list the
 * rest. A picture (the cover, the snap) is whole in it at its own shape,
 * its foot on the box's, so the details over that foot stand at one height
 * for every shape: A's hint inset from the foot, the facts on it, inset from
 * the picture's left and right. My pick
 * in the List View lab, 2026-11-23: "FUll height, show the details on top
 * of the video in a very subtle way".
 *
 * Pure: no font, no renderer. tools/listbox-check.c holds the boxes on
 * the 4:3 canvas and the Smart Pro's. */
#include <SDL.h>

/* Rows in sight, and a row: meta's 37 and room either side */
#define GL_ROWS  8
#define GL_ROW_H 56

typedef struct {
	SDL_Rect list;       /* the list's column: its text from x, rows from y */
	SDL_Rect media;      /* the media's box, its foot the rows' */
	int a_y;             /* A's hint's middle line */
	int a_gap;           /* the last fact's line to A's */
} gl_boxes;

/* The boxes for a canvas `screen_w` wide whose shelf heading's line is
 * `sys` (games_frame().sys) */
gl_boxes gl_boxes_for(SDL_Rect sys, int screen_w, int screen_h);

/* A picture `w` x `h` whole in the media's box at its own shape, its middle
 * on the box's and its foot on the box's foot */
SDL_FRect gl_media_fit(const gl_boxes *b, float w, float h);

/* The details' column over a picture at `pic` (gl_media_fit's, or the
 * frame turning from the cover's to the snap's): inset DS_INSET from its
 * left and its right */
void gl_details_col(SDL_FRect pic, int *x, int *w);

#endif
