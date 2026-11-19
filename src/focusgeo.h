/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZYPEROS_FOCUSGEO_H
#define ZYPEROS_FOCUSGEO_H

/* The focus's marks on whole panel pixels (DESIGN.md 11.47, Highlight in
 * 3.5; my picks in the focus lab, 2026-11-19: "attention to details").
 *
 * The launcher draws in the 1024 x 768 design space. The Brick shows it a
 * unit a pixel. The Pixel 2 scales the whole picture to 640 x 480 with
 * linear filtering (src/device/pixel2.c), so a unit is 0.625 of a pixel and
 * an edge lands on a whole pixel only where the filter reads one color on
 * each side of it. Its output pixel i reads the picture at 1.6 i + 0.3, the
 * two units either side blended: over each 8 units (5 pixels) pixel edges 0,
 * 2 and 3 are exact at units 0, 3 and 5, and edges 1 and 4 are closest at
 * units 2 and 6, with a tenth of the next unit in the pixel beside. So a
 * mark is laid out in panel pixels - a ring of 2 and a gap of 2 on the Pixel
 * 2 - and each of its pixel edges is drawn at the unit that lands cleanest
 * (fg_unit); and the focused tile, which grows a little, grows to where its
 * own edges and its ring's land exact (fg_edge). A unit a pixel (the Brick,
 * this computer) is exact everywhere, and these change nothing there.
 *
 * Plain C, no SDL: tools/focusgeo-check.c holds it against the filter. */

#include <stdbool.h>

/* The panel's pixels a unit is: 0.625 on the Pixel 2, 1 on the Brick */
#define FG_K_PIXEL2 0.625f

/* A rect in units */
typedef struct { float x, y, w, h; } fg_rect;

/* The panel pixel unit `u` is drawn on, rounded */
int fg_px(float u, float k);

/* The unit edge that draws panel pixel edge `p` cleanest at `k` */
int fg_unit(int p, float k);

/* Whether pixel edge `p` lands exact at `k` (no blend either side) */
bool fg_exact(int p, float k);

/* A pixel edge near `p` (within two) from which every edge `p + dir *
 * off[i]` lands exact, or as many as can, the nearest if several do. `dir`
 * is -1 for a left or top edge (the marks go outward, left or up), 1 for a
 * right or bottom one. */
int fg_edge(int p, int dir, const int *off, int n, float k);

/* The focused tile at rest: `cell` grown by `grow` of its size, the same
 * whole panel pixels each side, its four edges moved by no more than two
 * pixels to where they and the marks round them (`out`, `n` pixel offsets
 * outward from each edge: a ring's gap and its outer edge, or a bar's top
 * and foot under the bottom edge) land exact. In panel pixels: x, y, w, h. */
typedef struct { int x, y, w, h; } fg_px_rect;
fg_px_rect fg_tile(fg_rect cell, float grow, float k, const int *side_out, int side_n,
                   const int *foot_out, int foot_n);

/* A pixel rect back in units, each edge where it lands cleanest */
fg_rect fg_units(fg_px_rect p, float k);

/* The ring round a tile: `gap` and `ring` panel pixels from the units given
 * (Highlight's Border, fine: 3 and 3 units, 2 and 2 pixels on the Pixel 2) */
int fg_px_len(float u, float k);

/* A tile's weight in the focus's glide (0 a step away, 1 the focus): its
 * grow, its lift and its name's color move with it, all on one curve */
float fg_grow_at(float grow, float w);

/* The step back of the tiles out of focus (Highlight, the lab's "the others
 * step back"): the share of ground over a cover, and the opacity of a
 * system's still, at weight `w` */
float fg_dim_cover(float w);
float fg_dim_still(float w);

#endif
